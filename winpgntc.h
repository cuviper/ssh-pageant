/*
 * Pageant client header.
 * Copyright (C) 2009  Josh Stone
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
#define SSH_AGENT_FAILURE 5

extern void *agent_query(void *in);

static inline int msglen(void *p) {
    return 4 + ntohl(*(uint32_t *)p);
}

#endif /* __WINPGNTC_H__ */
