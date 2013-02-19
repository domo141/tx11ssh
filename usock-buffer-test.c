#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -eu; trg=`basename "$0" .c`; rm -f "$trg"
 WARN="-Wall -Wno-long-long -Wstrict-prototypes -pedantic"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
 set_cc() { CC=$2; }
 CC=; case ${1-} in CC=*) ifs=$IFS; IFS==; set_cc $1; IFS=$ifs; shift; esac
 case ${1-} in '') set x -O2; shift; esac
 #case ${1-} in '') set x -ggdb; shift; esac
 xexec () { echo + "$@"; exec "$@"; }
 xexec ${CC:-gcc} -std=c99 $WARN -DSERVER -DDISPLAY "$@" -o "$trg" "$0"
 exit
 */
#endif
/*
 * $ usock-buffer-test.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2013 Tomi Ollila
 *          All rights reserved
 *
 * Created: Wed 06 Feb 2013 15:00:54 EET too
 * Last modified: Tue 19 Feb 2013 21:39:36 EET too
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define null ((void*)0)

void vwarn(const char * format, va_list ap);
void warn(const char * format, ...);
void die(const char * format, ...);

int xmkusock(void);
int xubind_listen(const char * path);
int xuconnect(const char * path);
int xfcntl(int fd, int cmd, int arg);

#define test_socket "/tmp/usock-buffer-test-socket"

int main(int argc, char * argv[])
{
    if (argc != 2)
        die("Usage: %s bufsize", argv[0]);
    int size = atoi(argv[1]);

    if (size < 1 || size > 32768)
        die("Size '%d' not between 1 and 32768");

    char * buf = (char *)malloc(size);
    if (buf == null)
        die("Out of Memory!");

    (void)unlink(test_socket);

    int ssd = xubind_listen(test_socket);
    switch (fork()) {
    case 0:
        // cygwin requires accept(2), otherwise connect(2) blocks.
        (void)accept(ssd, null, 0);
        sleep(2);
        exit(0);
    case -1:
        die("fork:");
    }
    // parent

    int sd = xuconnect(test_socket);

    int flags = xfcntl(sd, F_GETFL, 0);
    xfcntl(sd, F_SETFL, flags | O_NONBLOCK);

    int c = 0;
    int d = 0;
    while (1) {
        int l = write(sd, buf, size);
        if (l == size) {
            c += size;
            if (d != c / 1000) {
                write(1, ".", 1);
                d = c / 1000;
            }
            if (c < 1000 * 1000)
                continue;
        }
        if (l > 0)
            c += l;
        printf("\nwrote %d bytes. last write: %d (%2.2f * %d)\n", c, l,
               (float)c / size, size);
        break;
    }
    (void)unlink(test_socket);
    return 0;
}

/////////////////////////////////////////////////


void vwarn(const char * format, va_list ap)
{
    int error = errno;

    static char timestr[32];
    time_t t = time(0);
    struct tm * tm = localtime(&t);

    strftime(timestr, sizeof timestr, "%d/%H:%M:%S ", tm);

    vfprintf(stderr, format, ap);
    if (format[strlen(format) - 1] == ':')
        fprintf(stderr, " %s\n", strerror(error));
    else
        fputs("\n", stderr);
    fflush(stderr);
}
void warn(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vwarn(format, ap);
    va_end(ap);
}
void die(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vwarn(format, ap);
    va_end(ap);
    exit(1);
}

int xmkusock(void)
{
    int sd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sd < 0) die("socket:");
    return sd;
}

int xubind_listen(const char * path)
{
    int sd = xmkusock();
    int one = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX
    };
    strncpy(addr.sun_path, path, sizeof addr.sun_path);
    if (addr.sun_path[sizeof addr.sun_path - 1] != '\0')
        die("path '%s' too long", path);

    if (bind(sd, (struct sockaddr *)&addr, sizeof addr) < 0)
        die("bind:");

    if (listen(sd, 5) < 0)
        die("listen:");

    return sd;
}

int xuconnect(const char * path)
{
    int sd = xmkusock();
    int one = 1;
    setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof one);

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX
    };
    strncpy(addr.sun_path, path, sizeof addr.sun_path);
    if (addr.sun_path[sizeof addr.sun_path - 1] != '\0')
        die("path '%s' too long", path);

    if (connect(sd, (struct sockaddr *)&addr, sizeof addr) < 0)
        die("connect:");

    return sd;
}

int xfcntl(int fd, int cmd, int arg)
{
    int rv = fcntl(fd, cmd, arg);
    if (rv < 0)
        die("fcntl:");
    return rv;
}
