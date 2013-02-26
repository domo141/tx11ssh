#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -eu; trg=`basename "$0" .c`; rm -f "$trg" "$trg"
 WARN="-Wall -Wno-long-long -Wstrict-prototypes -pedantic"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wshadow -Wwrite-strings -Wcast-qual" # -Wconversion
 set_cc() { CC=$2; }
 CC=; case ${1-} in CC=*) ifs=$IFS; IFS==; set_cc $1; IFS=$ifs; shift; esac
 case ${1-} in
	p) set x -O2; shift ;;
	d) set x -ggdb -DDEVEL; shift ;;
	'') exec >&2; echo Enter:
	echo " sh $0 [CC=<cc>] d -- for devel compilation"
	echo " sh $0 [CC=<cc>] p -- for production compilation"
	echo " other options -- passed to '${CC:-gcc}' command line"
	exit 1
 esac
 xexec () { echo + "$@"; exec "$@"; }
 xexec ${CC:-gcc} -std=c99 $WARN -DSERVER -DDISPLAY "$@" -o "$trg" "$0"
 exit
 */
#endif
/*
 * $ tx11ssh.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2013 Tomi Ollila
 *          All rights reserved
 *
 * Created: Tue 05 Feb 2013 21:01:50 EET too
 * Last modified: Mon 25 Feb 2013 23:48:56 EET too
 */

/* LICENSE: 2-clause BSD license ("Simplified BSD License"):

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _POSIX_C_SOURCE 200112L // for S_ISSOCK
//efine _POSIX_C_SOURCE 200809L // for strdup

#if DEVEL
#define CIOVEC_HAX 1
#endif

// defined while developing; undef'd for production compilation
#if CIOVEC_HAX
#define writev xxwritev
#endif

#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h> // for htons/ntohs
#include <errno.h>

#if CIOVEC_HAX
#undef writev
struct ciovec {
    const void * iov_base;
    size_t iov_len;
};
ssize_t writev(int fd, const struct ciovec * iov, int iovcnt);
#endif

// clang -dM -E - </dev/null | grep __GNUC__  outputs '#define __GNUC__ 4'

#if (__GNUC__ >= 4)
#define GCCATTR_SENTINEL __attribute__ ((sentinel))
#else
#define GCCATTR_SENTINEL
#endif

#if (__GNUC__ >= 3)
#define GCCATTR_PRINTF(m, n) __attribute__ ((format (printf, m, n)))
#define GCCATTR_NORETURN __attribute__ ((noreturn))
#define GCCATTR_CONST    __attribute__ ((const))
#else
#define GCCATTR_PRINTF(m, n)
#define GCCATTR_NORETURN
#define GCCATTR_CONST
#endif


#define null ((void*)0)

#define BB {
#define BE }

const char server_ident[] = {
    '\0','r','x','1','1','-','s','e','r','v','e','r','-','0','\n'
};
const char display_ident[] = {
    '\0','r','x','1','1','-','d','i','s','p','l','a','y','-','0','\n'
};

#if DEVEL
/*#define d1(x) do {	write(2, __func__, strlen(__func__)); \
 *///			write(2, ": ", 2); warn x; } while (0)
#define d2(x) do { warn x; } while (0)
#define d1(x) do { warn x; } while (0)
//#define d1(x) do {} while (0)
#define d0(x) do {} while (0)
#define da(x) do { if(!x) die("line %d: %s...", __LINE__, #x); } while (0)
#define dx(x) do { x } while (0);
#else
#define d2(x) do {} while (0)
#define d1(x) do {} while (0)
#define d0(x) do {} while (0)
#define da(x) do {} while (0)
#define dx(x) do {} while (0)
#endif

// byte1: chnl (5-255), byte2: chnlcntr, byte 3-4 msg length -- max 16384

struct {
    const char * component_ident;
    int component_identlen;
    int infolevel;
    char socket_file[24];

    struct pollfd pfds[256];
    unsigned char chnl2pfd[256];
    unsigned char chnlcntr[256];
    int nfds;
} G;

const char sockdir[] = "/tmp/.X11-unix";

void die(const char * format, ...) GCCATTR_PRINTF(1,2) GCCATTR_NORETURN;

void set_ident(const char * ident)
{
    G.component_ident = ident;
    G.component_identlen = strlen(G.component_ident);
}

void init_G(const char * ident)
{
    memset(&G, 0, sizeof G);

#if CIOVEC_HAX
    BB;
    struct iovec * iov = null;
    struct ciovec * ciov = null;

    if (sizeof *iov != sizeof *ciov)
	die("iovec size %lu different than ciovec size %lu",
	    sizeof *iov, sizeof *ciov);
    if (sizeof iov->iov_base != sizeof ciov->iov_base)
	die("iov_base sizes differ: %lu != %lu",
	    sizeof iov->iov_base, sizeof ciov->iov_base);
    if (sizeof iov->iov_len != sizeof ciov->iov_len)
	die("iov_len sizes differ: %lu != %lu",
	    sizeof iov->iov_len, sizeof ciov->iov_len);
    if ((const void *)&iov->iov_base != (const void *)&ciov->iov_base)
	die("iov_base offsets differ: %p != %p",
	    (void *)&iov->iov_base, (void *)&ciov->iov_base);
    if (&iov->iov_len != &ciov->iov_len)
	die("iov_len offsets differ: %p != %p",
	    (void *)&iov->iov_len, (void *)&ciov->iov_len);
    BE;
#endif

    set_ident(ident);
    G.infolevel = 3;
}

static void init_comm(void)
{
    const int n = sizeof G.pfds / sizeof G.pfds[0];

    for (int fd = 0; fd < n; fd++)
	G.pfds[fd].events = POLLIN;

    for (int fd = 5; fd < 256; fd++)
	(void)close(fd);

    signal(SIGPIPE, SIG_IGN);
}

void vout(int fd, const char * format, va_list ap)
{
    int error = errno;

    char timestr[32];
    char msg[1024];
    time_t t = time(0);
    struct tm * tm = localtime(&t);
#if CIOVEC_HAX
    struct ciovec iov[7];
#else
    struct iovec iov[7];
#endif
    int iocnt;

    strftime(timestr, sizeof timestr, "%d/%H:%M:%S ", tm);

    iov[0].iov_base = timestr;
    iov[0].iov_len = strlen(timestr);

    iov[1].iov_base = G.component_ident; // writev does not modify content !!!
    iov[1].iov_len = G.component_identlen;

    iov[2].iov_base = ": ";   // writev does not modify content !!!
    iov[2].iov_len = 2;

    vsnprintf(msg, sizeof msg, format, ap);

    iov[3].iov_base = msg;
    iov[3].iov_len = strlen(msg);

    if (format[strlen(format) - 1] == ':') {
	iov[4].iov_base = " ";   // writev does not modify content !!!
	iov[4].iov_len = 1;
	iov[5].iov_base = strerror(error);
	iov[5].iov_len = strlen(iov[5].iov_base);
	iov[6].iov_base = "\n";  // writev does not modify content !!!
	iov[6].iov_len = 1;
	iocnt = 7;
    }
    else {
	iov[4].iov_base = "\n";  // writev does not modify content !!!
	iov[4].iov_len = 1;
	iocnt = 5;
    }
    // This write is "opportunistic", so it's okay to ignore the
    // result. If this fails or produces a short write there won't
    // be any simple way to inform the user anyway. The probability
    // this happening without any other (visible) problems in system
    // is infinitesimally small..
    (void)writev(fd, iov, iocnt);
}

#define info1(...) if (G.infolevel >= 1) warn(__VA_ARGS__)
#define info2(...) if (G.infolevel >= 2) warn(__VA_ARGS__)
#define info3(...) if (G.infolevel >= 3) warn(__VA_ARGS__)
#define info4(...) if (G.infolevel >= 3) warn(__VA_ARGS__)

void warn(const char * format, ...) GCCATTR_PRINTF(1,2);
void warn(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);
    vout(2, format, ap);
    va_end(ap);
}
void die(const char * format, ...)
{
    va_list ap;

    close(0); close(1); close(3); close(4);

    va_start(ap, format);
    vout(2, format, ap);
    va_end(ap);
    exit(1);
}

void sleep100ms(void)
{
    d1(("sleep 100 msec (0.1 sec)"));
    poll(0, 0, 100);
}

void wait_for_ident(int fd, const char * ident, size_t isize)
{
#if 1
    char buf[8192];
#else
    char buf[7]; // for testing purposes
#endif
    int l = 0;
    size_t epos = 0;

    info2("Waiting identification");

    while (1) {
	l = read(fd, buf, sizeof buf);
	if (l <= 0) {
	    if (l < 0) {
		if (errno == EINTR)
		    continue;
		die("Read failed while waiting ident:");
	    }
	    die("EOF while waiting ident");
	}
	d1(("read %d bytes from %d (epos=%jd)", l, fd, epos));
	char * p = buf;
	if (epos == 0) {
	    p = memchr(p, ident[epos], l);
	    if (p == null) {
		write(1, buf, l);
		continue;
	    }
	    l -= (p - buf);
	}
	while (l > 0 && epos < isize) {
	    if (*p == ident[epos]) {
		p++;
		epos++;
		l--;
	    }
	    else {
		epos = 0;
		if (*p != ident[epos]) {
		    write(1, buf, p - buf);
		    break;
		}
	    }
	}
	if (epos == isize)
	    return; // XXX check that l == 0 //
    }
}

void xreadfully(int fd, void * buf, ssize_t len)
{
    int tl = 0;

    if (len <= 0)
	return;

    while (1) {
	int l = read(fd, buf, len);

	if (l == len)
	    return;

	if (l <= 0) {
	    if (l == 0)
		die("EOF from %d", fd);
	    if (errno == EINTR)
		continue;
	    else
		die("read(%d, ...) failed:", fd);
	}
	tl += l;
	buf = (char *)buf + l;
	len -= l;
    }
}

bool to_socket(int sfd, void * data, size_t datalen)
{
    ssize_t wlen = write(sfd, data, datalen);
    if (wlen == (ssize_t)datalen)
	return true;

    char * buf = (char *)data;
    int tries = 0;
    do {
	// here if socket buffer full (typically 100kb of data unread)
	// give peer 1/10 of a second to read it and retry.
	// POLLOUT might inform that there is room for new data but
	// write may still fail if the data doesn't fully fit ???
	sleep100ms();

	if (wlen < 0)
	    wlen = 0;

	buf += wlen;
	datalen -= wlen;

	wlen = write(sfd, buf, datalen);
	if (errno != EAGAIN && errno != EWOULDBLOCK) {
	    warn("Channel %d fd gone ...:", sfd);
	    return false;
	}

	if (wlen == (ssize_t)datalen) {
	    info2("Writing data to %d took %d tries", sfd, tries);
	    return true;
	}
	d1(("%d: wlen %d (of %u):", sfd, (int)wlen, (unsigned int)datalen));
    } while (++tries < 100); // 100 times makes that 10 sec total.

    info1("Peer at %d too slow to read traffic. dropping", sfd);
    return false;
}

void mux_eof_to_netpipe(int fd, int chnl)
{
    unsigned char hdr[4];
    hdr[0] = chnl;
    hdr[1] = G.chnlcntr[chnl];
    hdr[2] = 0;
    hdr[3] = 0;
    if (write(fd, hdr, 4) != 4)
	die("write() to net failed:");
}

void mux_to_netpipe(int fd, int chnl, const void * data, size_t datalen)
{
    uint16_t hdr[2];
#if CIOVEC_HAX
    struct ciovec iov[2];
#else
    struct iovec iov[2];
#endif
    ((unsigned char *)hdr)[0] = chnl;
    ((unsigned char *)hdr)[1] = G.chnlcntr[chnl];
    hdr[1] = htons(datalen);
    iov[0].iov_base = hdr;
    iov[0].iov_len = 4;
    iov[1].iov_base = data;
    iov[1].iov_len = datalen;

    d0(("%jd bytes to chnl %d via fd %d", datalen, chnl, fd));

    if (writev(fd, iov, 2) != (ssize_t)datalen + 4)
	die("writev() to net failed:");
}

int from_netpipe(int fd, uint8_t * chnl, uint8_t * cntr, char * data)
{
    uint16_t hdr[2];

    xreadfully(fd, hdr, 4);
    *chnl = ((unsigned char *)hdr)[0];
    *cntr = ((unsigned char *)hdr)[1];
    int dlen = ntohs(hdr[1]);
    if (dlen > 16384)
	die("Protocol error: server message '%d' too long\n", dlen);

    d0(("%d bytes for channel %d", dlen, *chnl));

    if (dlen)
	xreadfully(fd, data, dlen);
    return dlen;
}

void close_socket_and_remap(int sd)
{
    close(sd);
    G.nfds--;
    int o = G.chnl2pfd[sd];
    G.chnl2pfd[sd] = 0;

    G.chnlcntr[sd]++;

    d2(("remap: sd %d, o %d, nfds %d, cntr %d", sd, o, G.nfds,G.chnlcntr[sd]));

    if (o == G.nfds)
	return;

    G.pfds[o].fd = G.pfds[G.nfds].fd;
    G.pfds[o].revents = G.pfds[G.nfds].revents;

    G.chnl2pfd[G.pfds[o].fd] = o;

    dx(for (int i = 0; i < G.nfds; i++) { int fd = G.pfds[i].fd;
	    warn("fdmap: i: %d -> fd: %d (%d)", i, fd, G.chnl2pfd[fd]); });
}

int from_socket_to_netpipe(int pfdi, int netfd)
{
    char buf[16384];
    int fd = G.pfds[pfdi].fd;
    int len = read(fd, buf, sizeof buf);
    d0(("%d %d %d -- read %d", fd, pfdi, G.chnl2pfd[fd], len));
    if (len <= 0) {
	if (len < 0)
	    warn("read from %d failed, closing:", fd);
	else
	    // change to info or verbose tjsp, info hjuva
	    info2("EOF from %d. closing", fd);

	mux_eof_to_netpipe(netfd, fd);
	close_socket_and_remap(fd);
	return false;
    }
    mux_to_netpipe(netfd, fd, buf, len);
    return true;
}

int xmkusock(void)
{
    int sd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sd < 0) die("socket:");
    return sd;
}

void xdup2(int o, int n)
{
    if (dup2(o, n) < 0)
	die("dup2:");
}

int xfcntl(int fd, int cmd, int arg)
{
    int rv = fcntl(fd, cmd, arg);
    if (rv < 0) die("fcntl:");
    return rv;
}

void set_nonblock(int sd)
{
    xfcntl(sd, F_SETFL, xfcntl(sd, F_GETFL, 0) | O_NONBLOCK);
}

void remote_set_socket_file(char * arg)
{
    if (arg == null)
	die("socket argument missing");

    char * p = arg;
    while (isdigit((int)*p))
	p++;
    if (p == arg || *p != '\0')
	die("'%s': illegal display socket number", arg);
    if (p - arg > 4)
	die("'%s': display socket number too large", arg);

    snprintf(G.socket_file, sizeof G.socket_file, "%s/X%s", sockdir, arg);
}

#if SERVER

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
	die("Path '%s' too long", path);

    if (bind(sd, (struct sockaddr *)&addr, sizeof addr) < 0) {
	if (errno == EADDRINUSE)
	    die("bind: address already in use\n"
		"The socket '%s' exists and may be live\n"
		"Remove the file and try again if the socket is stale", path);
	die("bind:");
    }
    if (listen(sd, 5) < 0)
	die("listen:");

    return sd;
}

void create_nullfd(int fd)
{
    (void)close(fd);
    int nullfd = open("/dev/null", O_RDWR, 0);
    if (nullfd < 0)
	die("open('/dev/null'):");
    if (nullfd != fd)
	die("Unexpected fd %d (not %d) for nullfd", nullfd, fd);
}

int sshconn(char * ssh_command, char * argv[],
	    const char * component, const char * socknum)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
	die("socketpair:");

    switch (fork()) {
    case 0:
	/* child */
	close(sv[0]);
	xdup2(sv[1], 0);
	xdup2(sv[1], 1);
	if (sv[1] > 2)
	    close(sv[1]);

#define args_max 200
	char * av[args_max + 5];
	int i;
	argv--;
	for (i = 1; i < args_max && argv[i]; i++)
	    av[i] = argv[i];
	if (i == args_max)
	    die("Too many args for ssh (internal limit %d)", args_max);
#undef args_max
	av[0] = ssh_command;
	char cmd[] = { "tx11ssh" };
	av[i++] = cmd;
	char rmt[] = { "--remote--" };
	av[i++] = rmt;
	char idn[32];
	strcpy(idn, component);
	av[i++] = idn;
	char dnumstr[8];
	strcpy(dnumstr, socknum);
	av[i++] = dnumstr;
	av[i] = null;
	d0(("execvp %s [ %s, %s, %s, %s... ]", av[0],av[0],av[1],av[2],av[3]));
	execvp(ssh_command, av);
	die("execvp:");
    case -1:
	die("fork:");
    }
    /* parent */
    close(sv[1]);
    return sv[0];
}

void server_handle_display_message(int rfdi, int rfdo)
{
    char buf[16384];
    unsigned char chnl, cntr;

    int len = from_netpipe(rfdi, &chnl, &cntr, buf);

    d1(("chnl %d, cntr %d, len %d (%d)", chnl, cntr, len, G.nfds));

    if (cntr != G.chnlcntr[chnl]) {
	info3("Message to old channel %d (cntr %d != %d) (%d bytes). dropped",
	     chnl, cntr, G.chnlcntr[chnl], len);
	return;
    }
    if (len == 0) {
	info2("EOF from %d. closing", chnl);
	close_socket_and_remap(chnl);
	return;
    }
    if (! to_socket(chnl, buf, len)) {
	mux_eof_to_netpipe(rfdo, chnl);
	close_socket_and_remap(chnl);
    }
}


void server_atexit(void)
{
    close(4);
    unlink(G.socket_file);
}

void server_loop(int rfdi) GCCATTR_NORETURN;
void server_loop(int rfdi)
{
    if (mkdir(sockdir, 01777) == 0)
	chmod(sockdir, 01777);
    (void)close(4);
    int ssd = xubind_listen(G.socket_file);

    atexit(server_atexit);

    if (ssd != 4)
	die("Unexpected fd '%d' (not 4) for server socket", ssd);

    init_comm();

    G.pfds[0].fd = rfdi;
    G.pfds[1].fd = 4;
    G.nfds = 2;

    info2("Initialization done");

    // if stdin fd 0, then use stdout (fd 1)
    int rfdo = rfdi? rfdi: 1;

    while (1)
    {
	d0(("before poll: nfds = %d", G.nfds));
	int n;
	if ((n = poll(G.pfds, G.nfds, -1)) <= 0)
	    exit(1);

	if (G.pfds[0].revents) {
	    server_handle_display_message(rfdi, rfdo);
	    if (n == 1)
		continue;
	}
	for (int i = 2; i < G.nfds; i++)
	    if (G.pfds[i].revents && ! from_socket_to_netpipe(i, rfdo))
		i--;

	// last add to fd table
	if (G.pfds[1].revents) { /* XXX should check POLLIN */
	    int sd = accept(4, null, 0);
	    d1(("%d = accept(4, ...)", sd));
	    if (sd > 255) {
		//if (G.nfds == sizeof G.pfds / sizeof G.pfds[0]) {
		warn("Connection limit reached");
		close(sd);
	    }
	    else {
		set_nonblock(sd);
		G.pfds[G.nfds].fd = sd;
		G.chnl2pfd[sd] = G.nfds++;
		d2(("new chnl %d, nfds %d", sd, G.nfds));
	    }
	}
    }
}

void remote_server(char ** av)
{
    set_ident("remote-s");

    remote_set_socket_file(av[1]);

    if (write(1, server_ident, sizeof server_ident) != sizeof server_ident)
	die("Could not send server ident:");

    wait_for_ident(0, display_ident, sizeof display_ident);

    write(1, "", 1);

    create_nullfd(3);

    server_loop(0);
}

void local_server(void) GCCATTR_NORETURN;
void local_server(void)
{
    set_ident("local-s");

    atexit(sleep100ms);

    wait_for_ident(3, display_ident, sizeof display_ident);

    if (write(3, server_ident, sizeof server_ident) != sizeof server_ident)
	die("Could not send server ident:");

    char c; xreadfully(3, &c, 1);

    server_loop(3);
}
#endif // SERVER

#if DISPLAY

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
	die("Path '%s' too long", path);

    if (connect(sd, (struct sockaddr *)&addr, sizeof addr) < 0) {
	int e = errno;
	warn("Connecting to '%s' failed...", path);
	warn("...%s. Returning EOF", strerror(e));
	close(sd);
	return -1;
    }
    d1(("connect fd %d", sd));

    return sd;
}

void display_handle_server_message(int rfdi, int rfdo)
{
    char buf[16384];
    unsigned char chnl, cntr;

    int len = from_netpipe(rfdi, &chnl, &cntr, buf);

    int pfdi = G.chnl2pfd[chnl];

    d1(("chnl %d, cntr %d, len %d, pfdi %d(/%d)", chnl,cntr,len,pfdi,G.nfds));

    if (cntr != G.chnlcntr[chnl]) {
	info3("Message to old channel %d (cntr %d != %d) (%d bytes). dropped",
	      chnl, cntr, G.chnlcntr[chnl], len);
	return;
    }
    if (pfdi == 0) {
	if (chnl < 5) {
	    warn("New connection %d less than 5. Drop it", chnl);
	    return;
	}
	if (len == 0) {
	    info1("New connection %d EOF'd immediately", chnl);
	    return;
	}
	int fd = xuconnect(G.socket_file);
	if (fd != chnl) {
	    if (fd < 0) {
		warn("Failed to connect %s:", G.socket_file);
		mux_eof_to_netpipe(rfdo, chnl);
		return;
	    }
	    xdup2(fd, chnl);
	    if (fd < chnl)
		xdup2(3, fd); // /dev/null to plug (remote stale or slow) fd
	    else
		close(fd);
	}
	set_nonblock(chnl);
	G.pfds[G.nfds].fd = chnl;
	G.chnl2pfd[chnl] = G.nfds++;
	d2(("new chnl %d, nfds %d", fd, G.nfds));
    }
    if (len == 0) {
	info2("EOF for %d. closing", chnl);
	close_socket_and_remap(chnl);
	return;
    }

    if (! to_socket(chnl, buf, len)) {
	mux_eof_to_netpipe(rfdo, chnl);
	close_socket_and_remap(chnl);
    }
}

void display_loop(int rfdi) GCCATTR_NORETURN;
void display_loop(int rfdi)
{
    int rfdo = rfdi? rfdi: 1;

    init_comm();

    G.pfds[0].fd = rfdi;
    G.nfds = 1;

    info2("Initialization done");
    while (1)
    {
	int n;
	if ((n = poll(G.pfds, G.nfds, -1)) <= 0)
	    exit(1);

	if (G.pfds[0].revents) { /* XXX should check POLLIN */
	    display_handle_server_message(rfdi, rfdo);
	    if (n == 1)
		continue;
	}

	for (int i = 1; i < G.nfds; i++)
	    if (G.pfds[i].revents && ! from_socket_to_netpipe(i, rfdo))
		i--;
    }
}

void remote_display(char ** av) GCCATTR_NORETURN;
void remote_display(char ** av)
{
    set_ident("remote-d");

    remote_set_socket_file(av[1]);

    if (write(1, display_ident, sizeof display_ident) != sizeof display_ident)
	die("Could not send server ident:");

    wait_for_ident(0, server_ident, sizeof server_ident);

    write(1, "", 1);

    create_nullfd(3);
    xdup2(3, 4);

    display_loop(0);
}

void local_display(void) GCCATTR_NORETURN;
void local_display(void)
{
    set_ident("local-d");

    atexit(sleep100ms);

    wait_for_ident(3, server_ident, sizeof server_ident);

    if (write(3, display_ident, sizeof display_ident) != sizeof display_ident)
	die("Could not send server ident:");

    char c; xreadfully(3, &c, 1);

    create_nullfd(4);
    display_loop(3);
}

#endif // DISPLAY

bool get_next_arg_val(int * acp, char *** avp, const char * s, char ** ap)
{
    int i = strcmp(**avp, s);

    if (i == 0) {
	(*acp)--; (*avp)++;
	*ap = **avp;
	return true;
    }
    if (i == '=') {
	int l = strlen(s);
	if (memcmp(**avp, s, l) == 0 && (**avp)[l] == '=') {
	    *ap = (**avp) + l + 1;
	    return true;
	}
    }
    return false;
}

void exit_sig(int sig) { exit(sig); }

int main(int argc, char ** argv)
{
    init_G(argv[0]);

    signal(SIGHUP, exit_sig);
    signal(SIGINT, exit_sig);
    signal(SIGTERM, exit_sig);

    for (int i = 1; i < argc; i++)
	if (strcmp(argv[i], "--remote--") == 0) {
	    i++;
	    if (argv[i] == null)
		die("No remote component name");

	    /**/ if (strcmp(argv[i], "server") == 0)
		remote_server(argv + i);
	    else if (strcmp(argv[i], "display") == 0)
		remote_display(argv + i);

	    die("'%s': unknown remote component", argv[i]);
	}

    if (argc < 2) {
	G.component_identlen = 0;
#define NL "\n"
	die("\n\nUsage: %s (+|-) [:[nums][:numd]] [--ssh-command command] args"
	    NL
	    NL "  +: open (X) windows to remote machine display after connecting"
	    NL "  -: open (X) windows from remote machine to local display after connecting"
	    NL
	    NL "  nums: server socket to bind, 11 by default"
	    NL "  numd: display socket to connect, 0 by default"
	    NL
	    NL "  --ssh-command: command instead of 'ssh' to use for tunnel"
	    NL
	    NL " args: see help of 'ssh' (or --ssh-command) for what arguments the command"
	    NL "       accepts. Note that all args aren't useful (like '-f' for ssh)."
	    NL, G.component_ident);
    }

    int remote_is_display;
    BB;

    argc--; argv++;

    /**/ if (argv[0][0] == '+' && argv[0][1] == '\0')
	remote_is_display = true;
    else if (argv[0][0] == '-' && argv[0][1] == '\0')
	remote_is_display = false;
    else
	die("Prefix/replace first arg '%s' with '+' or '-'", argv[0]);

    argc--; argv++;

    char * ssh_command = null;
    char ssh_default_cmd[8];
    const char * ssn = "11";
    const char * dsn = "0";

    // partial argument parsing block //
    for( ; argc >= 1; argc--, argv++) {
	if (argv[0][0] == ':') {
	    char * p = &argv[0][1];
	    if (isdigit((int)*p)) {
		ssn = p++;
		while (isdigit((int)*p))
		    p++;
		if (*p != ':' && *p != '\0')
		    die("Illegal X server socket number in '%s'", argv[0]);
		if (p - argv[0] > 5)
		    die("X server socket number in '%s' is too big", argv[0]);
	    }
	    if (*p == ':') {
		char * q = p++;
		dsn = p;
		while (isdigit((int)*p))
		    p++;
		if (p == dsn || *p != '\0')
		    die("Illegal X display socket number in '%s'", argv[0]);
		if (p - dsn > 4)
		    die("X display socket number in '%s' is too big", argv[0]);
		*q = '\0';
	    }
	    continue;
	}
	if (get_next_arg_val(&argc, &argv, "--ssh-command", &ssh_command))
	    continue;
	break;
    }
    if (ssh_command == null) {
	memcpy(ssh_default_cmd, "ssh", 4);
	ssh_command = ssh_default_cmd;
    }
    if (argv[0] == null)
	die("Args missing."
	    " See help of '%s' how to provide those", ssh_command);

    if (argv[0][0] == '-' && argv[0][1] == '-' && argv[0][2] != '\0')
	die("Unregognized option '%s'\n" "If you want to pass that to '%s',"
	    " prefix it with option '--'", argv[0], ssh_command);

    const char * lsn, *rsn, *rid;

    if (remote_is_display) {
	lsn = ssn;
	rsn = dsn;
	rid = "display";
	struct stat st;

	// this can only be checked when "x server" socket to be bound is local
	if (stat(G.socket_file, &st) == 0) {
	    if (S_ISSOCK(st.st_mode))
		warn("Socket file '%s' exists and may be live", G.socket_file);
	    else
		warn("File '%s' exists but it is not socket", G.socket_file);
	    die("Remove the file and try again if the file is stale");
	}
	if (errno != ENOENT)
	    die("Can not determine whether '%s' exists: (%s)\n" "Access"
		" to parent directory is needed and the file may not exist",
		G.socket_file, strerror(errno));
    }
    else {
	lsn = dsn;
	rsn = ssn;
	rid = "server";
    }

    snprintf(G.socket_file, sizeof G.socket_file, "%s/X%s", sockdir, lsn);

    (void)close(3);
    int sshfd = sshconn(ssh_command, argv, rid, rsn);

    if (sshfd != 3)
	die("Unexpected fd %d (not 3) for ssh connection", sshfd);
    BE;

    if (remote_is_display)
	local_server();
    else
	local_display();
}
