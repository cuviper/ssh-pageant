/*
 * ssh-pageant compatability header.
 * Copyright (C) 2014  Josh Stone
 *
 * This file is part of ssh-pageant, and is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 */

#ifndef __COMPAT_H__
#define __COMPAT_H__

// Cygwin now has UNIX_PATH_MAX, but used to be _LEN.
// MSYS is basically an old Cygwin, so it only has _LEN.
#include <sys/un.h>
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX UNIX_PATH_LEN
#endif

#ifdef __MSYS__

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

// MSYS doesn't have program_invocation_short_name.
// Take the easy way out and hard-code it.
static char ssh_pageant_name[] = "ssh-pageant";
static char *program_invocation_short_name = ssh_pageant_name;

// MSYS doesn't have a BSD err.h at all, but it's easy to approximate.
// These are simplified by assuming a string-literal fmt, never NULL.
#define err(eval, fmt, args...) \
    ({ warn(fmt, ##args); exit(eval); })
#define errx(eval, fmt, args...) \
    ({ warnx(fmt, ##args); exit(eval); })
#define warn(fmt, args...) \
    warnx(fmt ": %s", ##args, strerror(errno))
#define warnx(fmt, args...) \
    fprintf(stderr, "%s: " fmt "\n", program_invocation_short_name, ##args)

// MSYS doesn't have mkdtemp, but mktemp+mkdir is probably fine.
static char *
mkdtemp(char *template)
{
    char *path = mktemp(template);
    if (path && mkdir(template, S_IRWXU) == 0)
        return path;
    return NULL;
}

// MSYS doesn't have strlcpy, so guarantee strncpy is terminated.
static size_t
strlcpy(char *dst, const char *src, size_t size)
{
    strncpy(dst, src, size);
    dst[size - 1] = '\0';
    return strlen(src);
}

// MSYS doesn't have SOCK_CLOEXEC, so set it in a separate call.
#define SOCK_CLOEXEC	0x02000000

static int
socket_ext(int domain, int type, int protocol)
{
    int fd = socket(domain, type & ~SOCK_CLOEXEC, protocol);
    if (fd >= 0 && type & SOCK_CLOEXEC)
        fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
}
#define socket(d, t, p)  socket_ext(d, t, p)

static int
accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
    int fd = accept(sockfd, addr, addrlen);
    if (fd >= 0 && flags & SOCK_CLOEXEC)
        fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
}

#else /* __CYGWIN__ */

#include <err.h>

#endif

#endif /* __COMPAT_H__ */
