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

extern int agent_query(void *in, int inlen, void **out, int *outlen);

#endif /* __WINPGNTC_H__ */
