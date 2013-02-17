#if 0 /* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*-
 set -eu; trg=`basename "$0" .c`; rm -f "$trg" "$trg"-display
 WARN="-Wall -Wno-long-long -Wstrict-prototypes -pedantic"
 WARN="$WARN -Wcast-align -Wpointer-arith " # -Wfloat-equal #-Werror
 WARN="$WARN -W -Wshadow -Wwrite-strings -Wcast-qual" # -Wconversion
 case ${1-} in
	p) set x -O2; shift ;;
	d) set x -ggdb -DDEVEL; shift ;;
	'') exec 2>&1; echo Enter:
	echo " sh $0 d -- for devel compilation"
	echo " sh $0 p -- for production compilation"
	echo " other options -- passed to '${CC:-gcc}' command line"
	exit 1
 esac
 set -x
 ${CC:-gcc} -std=c99 $WARN -DSERVER -DDISPLAY "$@" -o "$trg" "$0"
 exit
 */
#endif
/*
 * $ rx11ssh.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2013 Tomi Ollila
 *          All rights reserved
 *
 * Created: Tue 05 Feb 2013 21:01:50 EET too
 * Last modified: Sun 17 Feb 2013 12:57:06 EET too
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
#define d1(x) do {	write(2, __func__, strlen(__func__)); \
			write(2, ": ", 2); warn x; } while (0)
#define d0(x) do {} while (0)
#define da(x) do { if(!x) die("line %d: %s...", __LINE__, #x); } while (0)
#else
#define d1(x) do {} while (0)
#define d0(x) do {} while (0)
#define da(x) do {} while (0)
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

    G.component_ident = ident;
    G.component_identlen = strlen(G.component_ident);
    G.infolevel = 2;
}

static void init_comm(void)
{
    const int n = sizeof G.pfds / sizeof G.pfds[0];

    for (int fd = 0; fd < n; fd++)
	G.pfds[fd].events = POLLIN;

    for (int fd = 5; fd < 256; fd++)
	(void)close(fd);
}

void vout(int fd, const char * format, va_list ap)
{
    int error = errno;

    char timestr[32];
    char msg[512];
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

void info(int level, const char * format, ...) GCCATTR_PRINTF(2,3);
void info(int level, const char * format, ...)
{
    va_list ap;

    if (level > G.infolevel)
	return;

    va_start(ap, format);
    vout(2, format, ap);
    va_end(ap);
}
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

    va_start(ap, format);
    vout(2, format, ap);
    va_end(ap);
    exit(1);
}

void sleep100ms(void)
{
    poll(0, 0, 100);
}

void expect_ident(int fd, const char * ident, size_t isize)
{
    char buf[640]; // 640 bytes ought to be enough for anyone ;)
    size_t r = 0;
    int l;

    info(2, "Waiting identification");

    while (1) {
	while ( (l = read(fd, buf + r, isize - r)) > 0) {
	    r += l;
	    d0(("read %d bytes from %d (r=%d, isize=%d)", l, fd, r, isize));
	    if (memcmp(buf, ident, r) == 0) {
		if (r == isize)
		    return;
		continue;
	    }
	    if (buf[r-1] == '\n') {
		buf[r-1] = '\\';
		buf[r++] = 'n';
	    }
	    buf[r] = 0;
	    die("Unexpected ident '%s'", buf);
	}
	if (l < 0) {
	    if (errno == EINTR)
		continue;
	    die("Read failed while reading ident:");
	}
	die("EOF while reading ident");
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

	if (wlen == (ssize_t)datalen) {
	    info(2, "Writing data to %d took %d tries", sfd, tries);
	    return true;
	}
    } while (++tries < 100); // 100 times makes that 10 sec total.

    warn("Peer at %d too slow to read traffic. dropping", sfd);
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

    d0(("%d bytes to chnl %d", datalen, chnl));

    if (writev(fd, iov, 2) != (ssize_t)datalen + 4)
	die("writev() to net failed:");
}

int from_net(int fd, unsigned char * chnl, char * data)
{
    uint16_t hdr[2];

    xreadfully(fd, hdr, 4);
    *chnl = ((unsigned char *)hdr)[0];
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

    if (o == G.nfds)
	return;

    // XXX check these -- add debug infooo //
    // voiko mappays siirtää alle nfds loop position //

    G.pfds[o].fd = G.pfds[G.nfds].fd;
    G.pfds[o].revents = G.pfds[G.nfds].revents;

    G.chnl2pfd[G.pfds[o].fd] = o;
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
	    warn("EOF from %d", fd);

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

void redirect_stderr(const char * stderr_file)
{
    if (stderr_file) {
	int fd = open(stderr_file, O_WRONLY|O_CREAT|O_APPEND, 0644);
	if (fd < 0)
	    die("open '%s' for writing:", stderr_file);
	xdup2(fd, 2);
	close(2);
    }
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

void create_2_nullfds(int fd1, int fd2)
{
    (void)close(fd1);
    int nullfd = open("/dev/null", O_RDWR, 0);
    if (nullfd < 0)
	die("open('/dev/null'):");
    if (nullfd != fd1)
	die("Unexpected fd %d (not %d) for nullfd", nullfd, fd1);
    xdup2(fd1, fd2);
}

int sshconn(char * ssh_command, char * argv[], const char * socknum)
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
	char * av[args_max + 4];
	int i;
	for (i = 1; i < args_max && argv[i]; i++)
	    av[i] = argv[i];
	if (i == args_max)
	    die("Too many args for ssh (internal limit %d)", args_max);
#undef args_max
	av[0] = ssh_command;
	char cmd[] = { "rx11ssh" };
	av[i++] = cmd;
	char idn[] = { "--display--" };
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

// vertaile toisen suunnan kans.
void handle_network_input(void)
{
    char buf[16384];
    unsigned char chnl;

    int len = from_net(3, &chnl, buf);

    if (len == 0) {
	warn("EOF from %d. closing", chnl);
	close_socket_and_remap(chnl);
	return;
    }
    if (! to_socket(chnl, buf, len)) {
	mux_eof_to_netpipe(3, chnl);
	close_socket_and_remap(chnl);
    }
}


void server_atexit(void)
{
    close(3);
    close(4);
    unlink(G.socket_file);
    sleep100ms();
}

void server_sighandler(int sig)
{
    exit(sig);
}

void server_main(int argc, char * argv[])
{
    init_G(argv[0]);

    BB;
    char * stderr_file = null;
    char * ssh_command = null;
    int detach = 0;
    char ssh_default_cmd[8];
    const char * lsn = "11";
    const char * rsn = "0";

    // partial argument parsing block //
    for( ; argc >= 2; argc--, argv++) {
	if (argv[1][0] == ':') {
	    char * p = &argv[1][1];
	    if (isdigit(*p)) {
		lsn = p++;
		while (isdigit(*p))
		    p++;
		if (*p != ':' && *p != '\0')
		    die("Illegal local socket number in '%s'", argv[1]);
		if (p - argv[1] > 5)
		    die("Remote socket number in '%s' is too large", argv[1]);
	    }
	    if (*p == ':') {
		char * q = p++;
		rsn = p;
		while (isdigit(*p))
		    p++;
		if (p == rsn || *p != '\0')
		    die("Illegal remote socket number in '%s'", argv[1]);
		if (p - rsn > 4)
		    die("Remote socket number in '%s' is too large", argv[1]);
		*q = '\0';
	    }
	    continue;
	}
	if (strcmp(argv[1], "--detach") == 0) {
	    argv[1] = argv[0];
	    detach = 1;
	    continue;
	}
	if (strcmp(argv[1], "--stderr") == 0) {
	    stderr_file = argv[2];
	    argv[2] = argv[0];
	    argc--; argv++;
	    continue;
	}
	if (strcmp(argv[1], "--ssh-command") == 0) {
	    ssh_command = argv[2];
	    argv[2] = argv[0];
	    argc--; argv++;
	    continue;
	}
	if (strcmp(argv[1], "--") == 0) {
	    argv[1] = argv[0];
	    argc--; argv++;
	    break;
	}
	if (argv[1][0] == '-' && argv[1][1] == '-')
	    die("Unregognized option '%s'\n" "If you want to pass that to '%s'"
		", prefix it with option '--'",
		argv[1], ssh_command? ssh_command: "ssh");
	break;
    }

    if (detach && ! stderr_file)
	die("Option '--detach' requires option '--stderr'");

    if (argc < 2) {
	// XXX also separate case where ssh_command != null
	warn("See usage of 'ssh' for how to connect to display\n");
	execlp("ssh", "ssh", null);
	die("execvp:");
    }

    if (ssh_command == null) {
	memcpy(ssh_default_cmd, "ssh", 4);
	ssh_command = ssh_default_cmd;
    }

    snprintf(G.socket_file, sizeof G.socket_file, "%s/X%s", sockdir, lsn);
    struct stat st;

    if (stat(G.socket_file, &st) == 0) {
	if (S_ISSOCK(st.st_mode))
	    warn("Socket file '%s' exists and may be live", G.socket_file);
	else
	    warn("File '%s' exists but it is not socket", G.socket_file);
	die("Remove the file and try again if the file is stale");
    }
    if (errno != ENOENT)
	die("Can not determine whether '%s' exists: (%s)\n"
	    "Access to parent directory is needed and the file may not exist",
	    G.socket_file, strerror(errno));

    atexit(server_atexit);
    signal(SIGHUP, server_sighandler);
    signal(SIGINT, server_sighandler);
    signal(SIGTERM, server_sighandler);

    (void)close(3);
    int sshfd = sshconn(ssh_command, argv, rsn);

    if (sshfd != 3)
	die("Unexpected fd %d (not 3) for ssh connection", sshfd);

    G.component_ident = "local";
    G.component_identlen = strlen(G.component_ident);

    if (write(3, server_ident, sizeof server_ident) != sizeof server_ident)
	die("Could not send server ident:");

    // swap the above and this and forward IO in expect_ident... //
    expect_ident(3, display_ident, sizeof display_ident);

    (void)mkdir(sockdir, 1777);

    (void)close(4);

    int ssd = xubind_listen(G.socket_file);

    if (ssd != 4)
	die("Unexpected fd '%d' (not 4) for server socket", ssd);

    init_comm();

    G.pfds[0].fd = 3;
    G.pfds[1].fd = 4;
    G.nfds = 2;

    info(2, "Initialization done");

    if (detach) {
	int pipefd[2];
	if (pipe(pipefd) < 0)
	    die("pipe:");
	switch (fork())
	{
	case 0:
	    // child
	    close(pipefd[0]);
	    create_2_nullfds(0, 1);
	    da((stderr_file != null));
	    redirect_stderr(stderr_file);
	    setsid();
	    close(pipefd[1]);
	case -1:
	    die("fork:");
	default:
	    // parent
	    close(pipefd[1]);
	    // wait for the child to be ready
	    read(pipefd[0], (char *)pipefd, 1);
	    _exit(0);
	}
    }
    else
	redirect_stderr(stderr_file); // if stderr_file != null
    BE;

    while (1)
    {
	d0(("before poll: nfds = %d", G.nfds));
	int n;
	if ((n = poll(G.pfds, G.nfds, -1)) <= 0)
	    break;

	if (G.pfds[0].revents) {
	    handle_network_input();
	    if (n == 1)
		continue;
	}
	for (int i = 2; i < G.nfds; i++)
	    if (G.pfds[i].revents && ! from_socket_to_netpipe(i, 3))
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
		G.pfds[G.nfds].fd = sd;
		G.chnl2pfd[sd] = G.nfds++;
	    }
	}
    }
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

void handle_server_message(void)
{
    char buf[16384];
    unsigned char chnl;

    int len = from_net(0, &chnl, buf);

    int pfdi = G.chnl2pfd[chnl];

    d0(("%d %d %d", chnl, len, pfdi));

    if (pfdi == 0) {
	if (chnl < 5) {
	    warn("New connection %d less than 5. Drop it", chnl);
	    return;
	}
	if (len == 0) {
	    warn("New connection %d EOF'd immediately", chnl);
	    return;
	}
	int fd = xuconnect(G.socket_file);
	if (fd != chnl) {
	    if (fd < 0) {
		mux_eof_to_netpipe(1, chnl);
		return;
	    }
	    xdup2(fd, chnl);
	    if (fd < chnl)
		xdup2(3, fd); // /dev/null to plug (remote stale or slow) fd
	    else
		close(fd);
	}
	G.pfds[G.nfds].fd = chnl;
	G.chnl2pfd[chnl] = G.nfds++;
    }
    if (len == 0) {
	warn("EOF for %d. closing", chnl);
	close_socket_and_remap(chnl);
	return;
    }

    if (! to_socket(chnl, buf, len)) {
	mux_eof_to_netpipe(1, chnl);
	close_socket_and_remap(chnl);
    }
}

void display_main(int argc, char * argv[], int argi)
{
    init_G("remote");

    argc-= argi;
    argv+= argi;

    if (argc <= 1)
	die("Display socket argument missing");

    BB;
    char * p = argv[1];
    while (isdigit(*p))
	p++;
    if (p == argv[1] || *p != '\0')
	die("'%s': illegal display socket number", argv[1]);
    if (p - argv[1] > 4)
	die("'%s': display socket number too large", argv[1]);

    snprintf(G.socket_file, sizeof G.socket_file, "%s/X%s", sockdir, argv[1]);
    BE;

    if (write(1, display_ident, sizeof display_ident) != sizeof display_ident)
	die("Could not send display ident:");

    expect_ident(0, server_ident, sizeof server_ident);

    // same channel fd:s for both components -- & nullfd...
    create_2_nullfds(3, 4);

    init_comm();

    //G.pfds[0].fd = 0;
    G.nfds = 1;

    info(2, "Initialization done");
    while (1)
    {
	int n;
	if ((n = poll(G.pfds, G.nfds, -1)) <= 0)
	    break;

	if (G.pfds[0].revents) { /* XXX should check POLLIN */
	    handle_server_message();
	    if (n == 1)
		continue;
	}

	for (int i = 1; i < G.nfds; i++)
	    if (G.pfds[i].revents && ! from_socket_to_netpipe(i, 1))
		i--;
    }
}
#endif // DISPLAY

int main(int argc, char ** argv)
{
    for (int i = 1; i < argc; i++)
	if (strcmp(argv[i], "--display--") == 0) {
	    display_main(argc, argv, i);
	    return 0;
	}
    server_main(argc, argv);
    return 0;
}
