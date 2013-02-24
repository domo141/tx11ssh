#if 0 /*
	archos=`uname -s -m | awk '{ print tolower($2) "-" tolower($1) }'`
	libc_so=`ldd /usr/bin/env | awk '/libc.so/ { print $3 }'`
	bn=`basename "$0" .c`; lbn=$bn-$archos dlbn=$bn-dbg-$archos
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
 * $ libpreload_wrapx11sock.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2013 Tomi Ollila
 *          All rights reserved
 *
 * Created: Sun 24 Feb 2013 17:42:17 EET too
 * Last modified: Sun 24 Feb 2013 22:21:41 EET too
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include <fcntl.h>
#include <errno.h>

#include <dlfcn.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

const char server_socket_to_be_wrapped[] = "/tmp/.X11-unix/X11";

char server_socket_wrapped_path[sizeof ((struct sockaddr_un *)0)->sun_path];
int server_socket_path_set = 0;

#if (__GNUC__ >= 3)
static void die1(const char * emsg) __attribute__ ((noreturn));
static void die3(const char * m1, const char * m2, const char * m3)
    __attribute__ ((noreturn));
#endif

static void die1(const char * emsg)
{
    // writev()... :) ! //
    write(2, emsg, strlen(emsg));
    write(2, "\n", 1);
    exit(1);
}

// äh, too complex.
static void die3(const char * m1, const char * m2, const char * m3)
{
    // writev()... :) ! //
    write(2, m1, strlen(m1));
    // check for '
    write(2, " ", 1);
    write(2, m2, strlen(m2));
    if (m3) {
        write(2, " ", 1);
        write(2, m3, strlen(m3));
    }
    write(2, "\n", 1);
    exit(1);
}


static void * dlhandle = NULL;

static void dlopen_libc(void)
{
    if (dlhandle)
        return;

    dlhandle = dlopen(LIBC_SO, RTLD_LAZY);

    if (!dlhandle)
        die3("dlopening libc:", dlerror(), NULL);
}

static void * dlsym_(const char * symbol)
{
    void * sym = dlsym(dlhandle, symbol);
    char * str = dlerror();

    if (str != NULL)
        die3("finding symbol: ", symbol, str);

    return sym;
}

static void set_server_socket_path(void)
{
    const char * home = getenv("HOME");
    if (home == NULL)
        die1("No 'HOME' in environment");

    server_socket_wrapped_path[sizeof server_socket_wrapped_path-2] = '\0';

    snprintf(server_socket_wrapped_path, sizeof server_socket_wrapped_path,
             "%s/.ssh/X11", home);
    if (server_socket_wrapped_path[sizeof server_socket_wrapped_path-2] !='\0')
        die3("'HOME' string '",  home, "' in environment too long");

    server_socket_path_set = 1;
}

int stat(const char * path, struct stat * buf)
{
    static int (*stat_orig)(const char *, struct stat *) = NULL;

    if (! stat_orig)
    {
        dlopen_libc();
        *(void **)(&stat_orig) = dlsym_("stat");
    }

    if (strcmp(path, server_socket_to_be_wrapped) != 0)
        return stat_orig(path, buf);

    if (! server_socket_path_set)
        set_server_socket_path();

    return stat_orig(server_socket_wrapped_path, buf);
}

int unlink(const char * pathname)
{
    static int (*unlink_orig)(const char *) = NULL;

    if (! unlink_orig)
    {
        dlopen_libc();
        *(void **)(&unlink_orig) = dlsym_("unlink");
    }

    if (strcmp(pathname, server_socket_to_be_wrapped) != 0)
        return unlink_orig(pathname);

    if (! server_socket_path_set)
        set_server_socket_path();

    return unlink_orig(server_socket_wrapped_path);
}

int connect(int sockfd, const struct sockaddr * addr, socklen_t addrlen)
{
    static int (*connect_orig)(int, const struct sockaddr *, socklen_t) = NULL;

    if (! connect_orig)
    {
        dlopen_libc();
        /* connect_orig = dlsym(handle, "connect"); */
        /* connect_orig = (int(*)(int, ...))dlsym(handle, "connect"); */
        *(void **)(&connect_orig) = dlsym_("connect");
    }
    if (addr->sa_family != AF_UNIX)
        return connect_orig(sockfd, addr, addrlen);

    if (strcmp(((const struct sockaddr_un*)addr)->sun_path,
	       server_socket_to_be_wrapped) != 0)
        return connect_orig(sockfd, addr, addrlen);

    if (! server_socket_path_set)
        set_server_socket_path();

    struct sockaddr_un uaddr;
    memcpy(&uaddr, addr, sizeof uaddr);
    strcpy(uaddr.sun_path, server_socket_wrapped_path);

    return connect_orig(sockfd, (struct sockaddr *)&uaddr, sizeof uaddr);
}

int bind(int sockfd, const struct sockaddr * addr, socklen_t addrlen)
{
    static int (*bind_orig)(int, const struct sockaddr *, socklen_t) = NULL;

    if (! bind_orig)
    {
        dlopen_libc();
        *(void **)(&bind_orig) = dlsym_("bind");
    }

    return bind_orig(sockfd, addr, sizeof addrlen);
}
