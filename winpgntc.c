/*
 * Pageant client code.
 * Copyright (C) 2009, 2011  Josh Stone
 *
 * This file is part of ssh-pageant, and is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This file is derived from part of the PuTTY program, whose original
 * license is available in COPYING.PuTTY.
 */

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "winpgntc.h"

#define AGENT_COPYDATA_ID 0x804e50ba   /* random goop */

#define SSH_AGENT_FAILURE 5

static PSID
get_user_sid(void)
{
    HANDLE proc = NULL, tok = NULL;
    TOKEN_USER *user = NULL;
    DWORD toklen, sidlen;
    PSID sid = NULL, ret = NULL;

    if ((proc = OpenProcess(MAXIMUM_ALLOWED, FALSE, GetCurrentProcessId()))
            && OpenProcessToken(proc, TOKEN_QUERY, &tok)
            && (!GetTokenInformation(tok, TokenUser, NULL, 0, &toklen)
                && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            && (user = (TOKEN_USER *)LocalAlloc(LPTR, toklen))
            && GetTokenInformation(tok, TokenUser, user, toklen, &toklen)) {
        sidlen = GetLengthSid(user->User.Sid);
        sid = (PSID)malloc(sidlen);
        if (sid && CopySid(sidlen, sid, user->User.Sid)) {
            /* Success. Move sid into the return value slot, and null it out
             * to stop the cleanup code freeing it. */
            ret = sid;
            sid = NULL;
        }
    }

    if (proc != NULL)
        CloseHandle(proc);
    if (tok != NULL)
        CloseHandle(tok);
    LocalFree(user);
    free(sid);

    return ret;
}

void
agent_query(void *buf)
{
    HWND hwnd = FindWindow("Pageant", "Pageant");
    if (hwnd) {
        char mapname[] = "PageantRequest12345678";
        sprintf(mapname, "PageantRequest%08x", (unsigned)GetCurrentThreadId());

        PSECURITY_DESCRIPTOR psd = NULL;
        SECURITY_ATTRIBUTES sa, *psa = NULL;
        PSID usersid = get_user_sid();
        if (usersid) {
            psd = (PSECURITY_DESCRIPTOR)
                LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
            if (psd) {
                if (InitializeSecurityDescriptor
                        (psd, SECURITY_DESCRIPTOR_REVISION)
                        && SetSecurityDescriptorOwner(psd, usersid, FALSE)) {
                    sa.nLength = sizeof(sa);
                    sa.bInheritHandle = TRUE;
                    sa.lpSecurityDescriptor = psd;
                    psa = &sa;
                }
            }
        }

        HANDLE filemap = CreateFileMapping(INVALID_HANDLE_VALUE, psa,
                                           PAGE_READWRITE, 0,
                                           AGENT_MAX_MSGLEN, mapname);

        if (filemap != NULL && filemap != INVALID_HANDLE_VALUE) {
            void *p = MapViewOfFile(filemap, FILE_MAP_WRITE, 0, 0, 0);
            memcpy(p, buf, msglen(buf));

            COPYDATASTRUCT cds = {
                .dwData = AGENT_COPYDATA_ID,
                .cbData = 1 + strlen(mapname),
                .lpData = mapname,
            };

            int id = SendMessage(hwnd, WM_COPYDATA,
                                 (WPARAM) NULL, (LPARAM) &cds);

            if (msglen(p) > AGENT_MAX_MSGLEN)
                id = 0;

            if (id > 0)
                memcpy(buf, p, msglen(p));

            UnmapViewOfFile(p);
            CloseHandle(filemap);
            LocalFree(psd);
            psd = NULL;
            free(usersid);
            usersid = NULL;
            if (id > 0)
                return;
        }

        /* LocalFree and free are fine with NULL, so null checks aren't
          * necessary. */
        LocalFree(psd);
        free(usersid);

    }

    static const char reply_error[5] = { 0, 0, 0, 1, SSH_AGENT_FAILURE };
    memcpy(buf, reply_error, msglen(reply_error));
}
