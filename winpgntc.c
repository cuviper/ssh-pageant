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

void *
agent_query(void *in)
{
    HWND hwnd;
    char mapname[] = "PageantRequest12345678";
    HANDLE filemap;
    void *p, *ret = NULL;
    int id;
    COPYDATASTRUCT cds;

    hwnd = FindWindow("Pageant", "Pageant");
    if (!hwnd)
        return NULL;
    sprintf(mapname, "PageantRequest%08x", (unsigned)GetCurrentThreadId());
    filemap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                0, AGENT_MAX_MSGLEN, mapname);
    if (filemap == NULL || filemap == INVALID_HANDLE_VALUE)
        return NULL;
    p = MapViewOfFile(filemap, FILE_MAP_WRITE, 0, 0, 0);
    memcpy(p, in, msglen(in));
    cds.dwData = AGENT_COPYDATA_ID;
    cds.cbData = 1 + strlen(mapname);
    cds.lpData = mapname;

    id = SendMessage(hwnd, WM_COPYDATA, (WPARAM) NULL, (LPARAM) &cds);
    if (id > 0) {
        ret = malloc(msglen(p));
        if (ret)
            memcpy(ret, p, msglen(p));
    }
    UnmapViewOfFile(p);
    CloseHandle(filemap);
    return ret;
}
