#if 0 /*
	archos=`uname -s -m | awk '{ print tolower($2) "-" tolower($1) }'`
	libc_so=`ldd /usr/bin/env | awk '/libc.so/ { print $3 }'`
	bn=`basename "$0" .c | sed s/ldpreload_//`; lbn=$bn-$archos
	trap 'rm -f $bn.o' 0
	WARN="-Wall -Wstrict-prototypes -pedantic -Wno-long-long"
	WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
	WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
	set -xeu
	gcc -std=c99 -fPIC -rdynamic -g -c $WARN "$0" -o "$bn.o" \
		-DARCHOS="\"$archos\"" -DLIBC_SO="\"$libc_so\""
	gcc -shared -Wl,-soname,$lbn.so -o $lbn.so $bn.o -lc -ldl
	exit
      */
#endif
/*
 * $ ldpreload_fake_isatty0.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2013 Tomi Ollila
 *          All rights reserved
 *
 * Created: Wed 27 Feb 2013 22:54:11 EET too
 * Last modified: Fri 01 Mar 2013 19:50:43 EET too
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/uio.h>
#include <dlfcn.h>

#include <unistd.h>
#include <termios.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#if (__GNUC__ >= 4)
void die(int ev, ...) __attribute__((sentinel)) __attribute__((noreturn));
#endif
void die(int ev, ...)
{
    struct iovec iov[10];
    va_list ap;

    va_start(ap, ev);
    int i = 0;
    for (char * s = va_arg(ap, char *); s; s = va_arg(ap, char *)) {
	iov[i].iov_base = s;
	iov[i].iov_len = strlen(s);
	i++;
	if (i == sizeof iov / sizeof iov[0])
	    break;
    }
    writev(2, iov, i);
    exit(ev);
}

static void * dlhandle = NULL;

static void dlopen_libc(void)
{
    if (dlhandle)
        return;

    dlhandle = dlopen(LIBC_SO, RTLD_LAZY);

    if (!dlhandle)
        die(1, "dlopening libc failed: ", dlerror(), NULL);
}

static void * dlsym_(const char * symbol)
{
    void * sym = dlsym(dlhandle, symbol);
    char * str = dlerror();

    if (str != NULL)
        die(1, "finding symbol '", symbol, "' failed: ", str, NULL);

    return sym;
}


int isatty(int fd)
{
    static int (*isatty_orig)(int) = NULL;

    if (! isatty_orig)
    {
        dlopen_libc();
        *(void **)(&isatty_orig) = dlsym_("isatty");
    }
    // fd 0 may be pipe/socket, fd2 should be the terminal (tx11ssh child ssh)
    if (fd == 0)
	fd = 2;
    return isatty_orig(fd);
}

#if 0
int ioctl(int fd, int request, int anything)
{
    static int (*ioctl_orig)(int, int, int) = NULL;

    //printf("ioctl: %d %x %x\n", fd, request, anything);

    if (! ioctl_orig)
    {
        dlopen_libc();
        *(void **)(&ioctl_orig) = dlsym_("ioctl");
    }
    // fd 0 may be pipe/socket, fd2 should be the terminal (tx11ssh child ssh)
    if (fd == 0)
	fd = 2;

    return ioctl_orig(fd, request, anything);
}
#endif

int tcgetattr(int fd, struct termios *termios_p)
{
    static int (*tcgetattr_orig)(int, struct termios *) = NULL;

    if (! tcgetattr_orig)
    {
        dlopen_libc();
        *(void **)(&tcgetattr_orig) = dlsym_("tcgetattr");
    }
    // fd 0 may be pipe/socket, fd2 should be the terminal (tx11ssh child ssh)
    if (fd == 0)
	fd = 2;

    return tcgetattr_orig(fd, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
    static int (*tcsetattr_orig)(int, int, const struct termios *) = NULL;

    if (! tcsetattr_orig)
    {
        dlopen_libc();
        *(void **)(&tcsetattr_orig) = dlsym_("tcsetattr");
    }
    // fd 0 may be pipe/socket, fd2 should be the terminal
    if (fd == 0)
	fd = 2;

    return tcsetattr_orig(fd, optional_actions, termios_p);
}
