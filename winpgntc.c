/*
 * Pageant client code.
 * Copyright (C) 2009  Josh Stone
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

void
agent_query(void *buf)
{
    HWND hwnd = FindWindow("Pageant", "Pageant");
    if (hwnd) {
        char mapname[] = "PageantRequest12345678";
        sprintf(mapname, "PageantRequest%08x", (unsigned)GetCurrentThreadId());

        HANDLE filemap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
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

            if (id > 0)
                return;
        }
    }

    static const char reply_error[5] = { 0, 0, 0, 1, SSH_AGENT_FAILURE };
    memcpy(buf, reply_error, msglen(reply_error));
}
