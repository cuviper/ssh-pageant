/*
 * Pageant client header.
 * Copyright (C) 2009,2014  Josh Stone
 *
 * This file is part of ssh-pageant, and is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 */

#ifndef __WINPGNTC_H__
#define __WINPGNTC_H__

#include <arpa/inet.h>

#define AGENT_MAX_MSGLEN  8192

#ifdef __MSYS__
// MSYS doesn't have stdint.h or uint32_t,
// but its ntohl wants unsigned long anyway.
typedef unsigned long uint32_t;
#endif

extern void agent_query(void *buf);

static inline int msglen(const void *p) {
    return 4 + ntohl(*(const uint32_t *)p);
}

#endif /* __WINPGNTC_H__ */
