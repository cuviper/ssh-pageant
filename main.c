/*
 * ssh-pageant main code.
 * Copyright (C) 2009  Josh Stone
 *
 * This file is part of ssh-pageant, and is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "winpgntc.h"


#define SSH_AGENT_FAILURE 5


#define FD_FOREACH(fd, set) \
    for (fd = 0; fd < FD_SETSIZE; ++fd) \
        if (FD_ISSET(fd, set))


static char tempdir[UNIX_PATH_LEN] = "";
static char sockpath[UNIX_PATH_LEN] = "";


__attribute__((noreturn)) static void
cleanup_exit(int ret)
{
    unlink(sockpath);
    rmdir(tempdir);
    exit(ret);
}


static void
cleanup_signal(int sig __attribute__((unused)))
{
    cleanup_exit(0);
}


static int
open_auth_socket()
{
    struct sockaddr_un addr;
    mode_t um;
    int fd;

    strlcpy(tempdir, "/tmp/ssh-XXXXXX", sizeof(tempdir));
    if (!mkdtemp(tempdir)) {
        perror("mkdtemp");
        cleanup_exit(1);
    }

    snprintf(sockpath, sizeof(sockpath), "%s/agent.%d", tempdir, getpid());
    sockpath[sizeof(sockpath) - 1] = '\0';

    fd = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        cleanup_exit(1);
    }

    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, sockpath, sizeof(addr.sun_path));
    um = umask(S_IXUSR | S_IRWXG | S_IRWXO);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        umask(um);
        cleanup_exit(1);
    }
    umask(um);

    if (listen(fd, 128) < 0) {
        perror("listen");
        cleanup_exit(1);
    }

    return fd;
}


static void
daemonize()
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        cleanup_exit(1);
    }
    if (pid > 0) {
        printf("SSH_AUTH_SOCK=%s; export SSH_AUTH_SOCK;\n", sockpath);
        printf("SSH_PAGEANT_PID=%d; export SSH_PAGEANT_PID;\n", pid);
        //printf("echo ssh-pageant pid %d\n", pid);
        exit(0);
    }

    if (setsid() < 0) {
        perror("setsid");
        cleanup_exit(1);
    }

    fclose(stdin);
    fclose(stdout);
    fclose(stderr);
}


int
main()
{
    static char reply_error[5] = { 0, 0, 0, 1, SSH_AGENT_FAILURE };

    int sockfd, fd;
    fd_set read_set, write_set;
    char buf[AGENT_MAX_MSGLEN];
    void *sendbuf[FD_SETSIZE] = { NULL };

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);

    signal(SIGINT, cleanup_signal);
    signal(SIGHUP, cleanup_signal);
    signal(SIGTERM, cleanup_signal);

    sockfd = open_auth_socket();
    FD_SET(sockfd, &read_set);

    daemonize();

    while (1) {
        fd_set do_read_set = read_set;
        fd_set do_write_set = write_set;
        if (select(FD_SETSIZE, &do_read_set, &do_write_set, NULL, NULL) < 0)
            cleanup_exit(1);

        FD_FOREACH(fd, &do_read_set) {
            if (fd == sockfd) {
                int s = accept(sockfd, NULL, 0);
                if (s >= FD_SETSIZE)
                    close(s);
                else if (s >= 0)
                    FD_SET(s, &read_set);
            }
            else {
                int len = read(fd, buf, sizeof(buf));
                if (len >= 4 && len == (int)msglen(buf)) {
                    sendbuf[fd] = agent_query(buf);
                    FD_SET(fd, &write_set);
                }
                else
                    close(fd);
                FD_CLR(fd, &read_set);
            }
        }

        FD_FOREACH(fd, &do_write_set) {
            void *reply = sendbuf[fd] ?: &reply_error;
            int len = write(fd, reply, msglen(reply));
            if (len == (int)msglen(reply))
                FD_SET(fd, &read_set);
            else
                close(fd);
            FD_CLR(fd, &write_set);
            free(sendbuf[fd]);
            sendbuf[fd] = NULL;
        }
    }
}
