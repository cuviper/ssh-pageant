/*
 * ssh-pageant main code.
 * Copyright (C) 2009-2014  Josh Stone
 *
 * This file is part of ssh-pageant, and is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 */

#include "compat.h"

#include <errno.h>
#include <getopt.h>
#include <process.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "winpgntc.h"


#define FD_FOREACH(fd, set) \
    for (fd = 0; fd < FD_SETSIZE; ++fd) \
        if (FD_ISSET(fd, set))


struct fd_buf {
    int recv, send;
    char buf[AGENT_MAX_MSGLEN];
};


static char cleanup_tempdir[UNIX_PATH_MAX] = "";
static char cleanup_sockpath[UNIX_PATH_MAX] = "";


static void cleanup_exit(int status) __attribute__((noreturn));
static void cleanup_warn(const char *prefix) __attribute__((noreturn, nonnull));
static void cleanup_signal(int sig) __attribute__((noreturn));

static void do_agent_loop(int sockfd) __attribute__((noreturn));



static void
cleanup_exit(int status)
{
    unlink(cleanup_sockpath);
    rmdir(cleanup_tempdir);
    exit(status);
}


static void
cleanup_warn(const char *prefix)
{
    warn("%s", prefix);
    cleanup_exit(1);
}


static void
cleanup_signal(int sig)
{
    // Most caught signals are basically just treated as exit notifiers,
    // but when a child exits, copy its exit status so ssh-pageant is more
    // effective as a command wrapper.
    int status = 0;
    if (sig == SIGCHLD && wait(&status) > 0) {
        if (WIFEXITED(status))
            status = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            status = 128 + WTERMSIG(status);
    }
    cleanup_exit(status);
}


// Create a temporary path for the socket.
static void
create_socket_path(char* sockpath, size_t len)
{
    char tempdir[] = "/tmp/ssh-XXXXXX";
    if (!mkdtemp(tempdir))
        cleanup_warn("mkdtemp");

    // NB: Don't set cleanup_tempdir until after it's created
    strlcpy(cleanup_tempdir, tempdir, sizeof(cleanup_tempdir));

    snprintf(sockpath, len, "%s/agent.%d", tempdir, getpid());
}


// Prepare the socket at the given path.
static int
open_auth_socket(const char* sockpath)
{
    struct sockaddr_un addr;
    mode_t um;
    int fd;

    fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        cleanup_warn("socket");

    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, sockpath, sizeof(addr.sun_path));
    um = umask(S_IXUSR | S_IRWXG | S_IRWXO);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        cleanup_warn("bind");
    umask(um);

    // NB: Don't set cleanup_sockpath until after it's bound
    strlcpy(cleanup_sockpath, sockpath, sizeof(cleanup_sockpath));

    if (listen(fd, 128) < 0)
        cleanup_warn("listen");

    return fd;
}


// Try to reuse an existing socket path.  For now, just being able to connect
// will be deemed good enough.  If it can't connect, but is still a socket, try
// to remove it.  Return 0 if the path was simply not connectible, else exit.
static int
reuse_socket_path(const char* sockpath)
{
    struct sockaddr_un addr;
    int fd;

    fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        cleanup_warn("socket");

    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, sockpath, sizeof(addr.sun_path));
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        // The sockpath is already accepting connections -- reuse!
        close(fd);
        return 1;
    }
    else if (errno == ENOENT)
        return 0;
    else if (errno == ECONNREFUSED) {
        // Either it's not listening, or not a socket at all.  If it was at
        // least a socket, remove it so it can be replaced.
        struct stat st;
        if (stat(sockpath, &st) < 0)
            cleanup_warn("stat");

        if (S_ISSOCK(st.st_mode)) {
            if (unlink(sockpath) < 0)
                cleanup_warn("unlink");
            return 0;
        }

        // Restore the errno before warning out.
        errno = ECONNREFUSED;
    }
    cleanup_warn("connect");
}


static int
agent_recv(int fd, struct fd_buf *p)
{
    int len = recv(fd, p->buf + p->recv, sizeof(p->buf) - p->recv, 0);
    if (len <= 0) {
        if (len < 0)
            warn("recv(%d)", fd);
        return -1;
    }

    p->recv += len;
    if (p->recv < 4 || p->recv < msglen(p->buf))
        return 0;

    if (p->recv > msglen(p->buf)) {
        warnx("recv(%d) = %d (expected %d)",
              fd, p->recv, msglen(p->buf));
        return -1;
    }

    agent_query(p->buf);
    p->send = 0;
    return 1;
}


static int
agent_send(int fd, struct fd_buf *p)
{
    int len = send(fd, p->buf + p->send, msglen(p->buf) - p->send, 0);
    if (len < 0) {
        warn("send(%d)", fd);
        return -1;
    }

    p->send += len;
    if (p->send < msglen(p->buf))
        return 0;

    if (p->send > msglen(p->buf)) {
        warnx("send(%d) = %d (expected %d)",
              fd, p->send, msglen(p->buf));
        return -1;
    }

    p->recv = 0;
    return 1;
}


static void
do_agent_loop(int sockfd)
{
    int fd;
    fd_set read_set, write_set;
    struct fd_buf *bufs[FD_SETSIZE] = { NULL };

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    FD_SET(sockfd, &read_set);

    while (1) {
        fd_set do_read_set = read_set;
        fd_set do_write_set = write_set;
        if (select(FD_SETSIZE, &do_read_set, &do_write_set, NULL, NULL) < 0)
            cleanup_warn("select");

        if (FD_ISSET(sockfd, &do_read_set)) {
            int s = accept4(sockfd, NULL, NULL, SOCK_CLOEXEC);
            if (s >= FD_SETSIZE) {
                warnx("accept: Too many connections");
                close(s);
            }
            else if (s < 0)
                warn("accept");
            else {
                bufs[s] = calloc(1, sizeof(struct fd_buf));
                if (!bufs[s]) {
                    warnx("calloc: No memory");
                    close(s);
                }
                else
                    FD_SET(s, &read_set);
            }
            FD_CLR(sockfd, &do_read_set);
        }

        FD_FOREACH(fd, &do_read_set) {
            int res = agent_recv(fd, bufs[fd]);
            if (res != 0) {
                FD_CLR(fd, &read_set);
                if (res < 0) {
                    close(fd);
                    free(bufs[fd]);
                    bufs[fd] = NULL;
                }
                else
                    FD_SET(fd, &write_set);
            }
        }

        FD_FOREACH(fd, &do_write_set) {
            int res = agent_send(fd, bufs[fd]);
            if (res != 0) {
                FD_CLR(fd, &write_set);
                if (res < 0) {
                    close(fd);
                    free(bufs[fd]);
                    bufs[fd] = NULL;
                }
                else
                    FD_SET(fd, &read_set);
            }
        }
    }
}


// Quote and escape a string for shell eval.
// Caller must free the result.
static char *
shell_escape(const char *s)
{
    // The pessimistic growth is *4, when every character is ' mapped to '\''.
    // (No need to be clever.)  Add room for outer quotes and terminator.
    size_t len = strlen(s);
    char *mem = calloc(len + 1, 4);
    if (!mem)
        return NULL;

    char c, *out = mem;
    *out++ = '\''; // open the string
    for (c = *s++; c; c = *s++) {
        if (c == '\'') {
            *out++ = '\''; // close,
            *out++ = '\\'; // escape
            *out++ = '\''; // the quote,
            *out++ = '\''; // reopen
        }
        else
            *out++ = c; // plain copy
    }
    *out++ = '\''; // close the string
    *out++ = '\0'; // terminate
    return mem;
}


int
main(int argc, char *argv[])
{
    char sockpath[UNIX_PATH_MAX] = "";
    static struct option long_options[] = {
        { "help", no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        { "reuse", no_argument, 0, 'r' },
        { 0, 0, 0, 0 }
    };

    int sockfd = -1;

    int opt;
    int opt_debug = 0;
    int opt_quiet = 0;
    int opt_kill = 0;
    int opt_reuse = 0;
    int opt_lifetime = 0;
    int opt_csh = !!strstr(getenv("SHELL") ?: "", "csh");

    while ((opt = getopt_long(argc, argv, "+hvcskdqa:rt:",
                              long_options, NULL)) != -1)
        switch (opt) {
            case 'h':
                printf("Usage: %s [options] [command [arg ...]]\n", program_invocation_short_name);
                printf("Options:\n");
                printf("  -h, --help     Show this help.\n");
                printf("  -v, --version  Display version information.\n");
                printf("  -c             Generate C-shell commands on stdout.\n");
                printf("  -s             Generate Bourne shell commands on stdout. (default)\n");
                printf("  -k             Kill the current %s.\n", program_invocation_short_name);
                printf("  -d             Enable debug mode.\n");
                printf("  -q             Enable quiet mode.\n");
                printf("  -a SOCKET      Create socket on a specific path.\n");
                printf("  -r, --reuse    Allow to reuse an existing -a SOCKET.\n");
                printf("  -t TIME        Limit key lifetime in seconds (not supported by Pageant).\n");
                return 0;

            case 'v':
                printf("ssh-pageant 1.3\n");
                printf("Copyright (C) 2009-2013  Josh Stone\n");
                printf("License GPLv3+: GNU GPL version 3 or later"
                       " <http://gnu.org/licenses/gpl.html>.\n");
                printf("This is free software:"
                       " you are free to change and redistribute it.\n");
                printf("There is NO WARRANTY, to the extent permitted by law.\n");
                return 0;

            case 'c':
                opt_csh = 1;
                break;

            case 's':
                opt_csh = 0;
                break;

            case 'k':
                opt_kill = 1;
                break;

            case 'd':
                opt_debug = 1;
                break;

            case 'q':
                opt_quiet = 1;
                break;

            case 'a':
                if (strlen(optarg) + 1 > sizeof(sockpath))
                    errx(1, "socket address is too long");
                strcpy(sockpath, optarg);
                break;

            case 'r':
                opt_reuse = 1;
                break;

            case 't':
                opt_lifetime = 1;
                break;

            case '?':
                errx(1, "try --help for more information");
                break;

            default:
                // shouldn't get here
                errx(2, "getopt returned unknown code %#X", opt);
                break;
        }

    if (opt_kill) {
        pid_t pid;
        const char *pidenv = getenv("SSH_PAGEANT_PID");
        if (!pidenv)
            errx(1, "SSH_PAGEANT_PID not set, cannot kill agent");
        pid = atoi(pidenv);
        if (kill(pid, SIGTERM) < 0)
            err(1, "kill(%d)", pid);
        if (opt_csh) {
            printf("unsetenv SSH_AUTH_SOCK;\n");
            printf("unsetenv SSH_PAGEANT_PID;\n");
        }
        else {
            printf("unset SSH_AUTH_SOCK;\n");
            printf("unset SSH_PAGEANT_PID;\n");
        }
        if (!opt_quiet)
            printf("echo ssh-pageant pid %d killed;\n", pid);
        return 0;
    }

    if (opt_reuse && !sockpath[0])
        errx(1, "socket reuse requires specifying -a SOCKET");

    if (opt_lifetime && !opt_quiet)
        warnx("option is not supported by Pageant -- t");

    signal(SIGINT, cleanup_signal);
    signal(SIGHUP, cleanup_signal);
    signal(SIGTERM, cleanup_signal);

    int p_sock_reused = opt_reuse && reuse_socket_path(sockpath);
    if (!p_sock_reused) {
        if (!sockpath[0])
            create_socket_path(sockpath, sizeof(sockpath));
        sockfd = open_auth_socket(sockpath);
    }

    // If the sockpath is actually reused, don't daemonize, don't set
    // SSH_PAGEANT_PID, and don't go into do_agent_loop(). Just set
    // SSH_AUTH_SOCK and exit normally.
    int p_daemonize = !(opt_debug || p_sock_reused);
    int p_set_pid_env = !p_sock_reused;

    if (optind < argc) {
        const char **subargv = (const char **)argv + optind;
        setenv("SSH_AUTH_SOCK", sockpath, 1);
        if (p_set_pid_env) {
            char pidstr[16];
            snprintf(pidstr, sizeof(pidstr), "%d", getpid());
            setenv("SSH_PAGEANT_PID", pidstr, 1);
        }
        signal(SIGCHLD, cleanup_signal);
        if (spawnvp(_P_NOWAIT, subargv[0], subargv) < 0)
            cleanup_warn(argv[optind]);
    }
    else {
        pid_t pid = p_daemonize ? fork() : getpid();
        if (pid < 0)
            cleanup_warn("fork");
        if (pid > 0) {
            char *escaped_sockpath = shell_escape(sockpath);
            if (!escaped_sockpath)
                cleanup_warn("shell_escape");
            if (opt_csh) {
                printf("setenv SSH_AUTH_SOCK %s;\n", escaped_sockpath);
                if (p_set_pid_env)
                    printf("setenv SSH_PAGEANT_PID %d;\n", pid);
            }
            else {
                printf("SSH_AUTH_SOCK=%s; export SSH_AUTH_SOCK;\n", escaped_sockpath);
                if (p_set_pid_env)
                    printf("SSH_PAGEANT_PID=%d; export SSH_PAGEANT_PID;\n", pid);
            }
            free(escaped_sockpath);
            if (p_set_pid_env && !opt_quiet)
                printf("echo ssh-pageant pid %d;\n", pid);
            if (p_daemonize)
                return 0;
        }
        else if (setsid() < 0)
            cleanup_warn("setsid");
        else
            fclose(stderr);
    }
    fclose(stdin);
    fclose(stdout);

    if (!p_sock_reused)
        do_agent_loop(sockfd);

    return 0;
}
