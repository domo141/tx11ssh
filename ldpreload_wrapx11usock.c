#if 0 /*
	archos=`uname -s -m | awk '{ print tolower($2) "-" tolower($1) }'`
	libc_so=`ldd /usr/bin/env | awk '/libc.so/ { print $3 }'`
	bn=`basename "$0" .c`; lbn=$bn-$archos
	#trap 'rm -f $bn.o' 0
	WARN="-Wall -Wstrict-prototypes -pedantic -Wno-long-long"
	WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
	WARN="$WARN -W -Wwrite-strings -Wcast-qual -Wshadow" # -Wconversion
	set -xeu
	gcc -std=c99 -shared -fPIC $WARN -o $lbn.so "$0" \
		-DARCHOS="\"$archos\"" -DLIBC_SO="\"$libc_so\"" -ldl
	exit
	# keep this code for a while in case there is some portability issues..
	#gcc -std=c99 -fPIC -rdynamic -g -c $WARN "$0" -o "$bn.o" \
	#	-DARCHOS="\"$archos\"" -DLIBC_SO="\"$libc_so\""
	#gcc -shared -Wl,-soname,$lbn.so -o $lbn.so $bn.o -lc -ldl
	#exit
      */
#endif
/*
 * $ ldpreload_wrapx11usock.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2013 Tomi Ollila
 *          All rights reserved
 *
 * Created: Sun 24 Feb 2013 17:42:17 EET too
 * Last modified: Sat 02 Nov 2013 14:34:46 +0200 too
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
//#include <fcntl.h>
#include <errno.h>

#include <dlfcn.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

const char server_socket_directory[]     = "/tmp/.X11-unix";
const char server_socket_to_be_wrapped[] = "/tmp/.X11-unix/X11";

char server_socket_wrapped_path[sizeof ((struct sockaddr_un *)0)->sun_path];
int server_socket_path_set = 0;

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
        die(1, "dlopening libc failed:", dlerror(), NULL);
}

static void * dlsym_(const char * symbol)
{
    void * sym = dlsym(dlhandle, symbol);
    char * str = dlerror();

    if (str != NULL)
        die(1, "finding symbol '", symbol, "' failed:", str, NULL);

    return sym;
}

static void set_server_socket_path(void)
{
    const char * home = getenv("HOME");
    if (home == NULL)
        die(1, "No 'HOME' in environment", NULL);

    server_socket_wrapped_path[sizeof server_socket_wrapped_path-2] = '\0';

    snprintf(server_socket_wrapped_path, sizeof server_socket_wrapped_path,
             "%s/.ssh/X11", home);
    if (server_socket_wrapped_path[sizeof server_socket_wrapped_path-2] !='\0')
        die(1, "'HOME' string '",  home, "' in environment too long", NULL);

    server_socket_path_set = 1;
}

int mkdir(const char * path, mode_t mode)
{
    static int (*mkdir_orig)(const char *, mode_t) = NULL;

    if (! mkdir_orig)
    {
        dlopen_libc();
        *(void **)(&mkdir_orig) = dlsym_("mkdir");
    }

    if (strcmp(path, server_socket_directory) == 0)
        return 0;

    return mkdir_orig(path, mode);
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

    return connect_orig(sockfd, (struct sockaddr *)&uaddr, addrlen);
}

int bind(int sockfd, const struct sockaddr * addr, socklen_t addrlen)
{
    static int (*bind_orig)(int, const struct sockaddr *, socklen_t) = NULL;

    if (! bind_orig)
    {
        dlopen_libc();
        *(void **)(&bind_orig) = dlsym_("bind");
    }

    if (strcmp(((const struct sockaddr_un*)addr)->sun_path,
	       server_socket_to_be_wrapped) != 0)
        return bind_orig(sockfd, addr, addrlen);

    if (! server_socket_path_set)
        set_server_socket_path();

    struct sockaddr_un uaddr;
    memcpy(&uaddr, addr, sizeof uaddr);
    strcpy(uaddr.sun_path, server_socket_wrapped_path);

    return bind_orig(sockfd, (struct sockaddr *)&uaddr, addrlen);
}
