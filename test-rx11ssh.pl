#!/usr/bin/perl
# -*- cperl -*-
# $ test-rx11ssh.pl $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2013 Tomi Ollila
#	    All rights reserved
#
# Created: Sun 17 Feb 2013 14:57:05 EET too
# Last modified: Mon 18 Feb 2013 18:01:52 EET too

use 5.8.1;
use strict;
use warnings;
use Socket;
use Time::HiRes qw/usleep ualarm/;
use Errno;

sub exit0() { $SIG{TERM} = sub {}; kill -15, $$; exit 0; };

$SIG{ALRM} = \&exit0;
$SIG{INT} = \&exit0;
$SIG{TERM} = \&exit0;
$SIG{HUP} = \&exit0;

$SIG{CHLD} = sub {
    my $cpid; while (($cpid = wait) < 0) {}; my $ec = int $? / 256;
    warn "proc $cpid exited: code $? ($ec)\n";
};

#$SIG{__DIE__} = sub { warn $_[0]; exit 1; };

select((select(STDERR), $| = 0)[$[]); # buffered stderr; msgs only at exits

sub xfork()
{
    my $pid = fork;
    die 'fork' unless defined $pid;
    return $pid;
}

# enter parameters in format *INFH, *OUTFH
sub copyio(**)
{
    my $buf;
    my $len = sysread $_[0], $buf, 16384;
    unless (defined $len) {
	print STDERR $$, ': ', $!;
	return 0;
    }
    if ($len == 0) {
	print STDERR $$, ': EOF while reading';
	return 0;
    }
    my $wlen = syswrite $_[1], $buf;
    unless ($wlen == $len) {
	print STDERR "$$: short write ($wlen < $len)";
	return 0;
    }
    return 1;
}

my $clisock = '/tmp/.X11-unix/X767';

if (@ARGV) {
    if ($ARGV[0] eq  "xkilent") {
	die "$clisock missing\n" unless -e $clisock;
	socket(S, AF_UNIX, SOCK_STREAM, 0) or die 'socket: ', $!;
	my $sockaddr = sockaddr_un($clisock);
	connect(S, $sockaddr) or die "connect to $clisock: ", $!;
	ualarm (3e5 + int rand 2e5);
	my $buf = '1234567890';
	while (1) {
	    die('syswrite: ', $!) unless syswrite(S, $buf, 10) == 10;
	    usleep(1e5 + int rand 1e5);
	    my $rlen = sysread(S, $buf, 1024);
	    die('sysread: ', $!) unless defined $rlen;
	    die('EOF') if $rlen == 0;
	}
	exit 0;
    }
    if ($ARGV[0] eq  "delayer") {
	socketpair(S1, S2, AF_UNIX, SOCK_STREAM, 0) or die 'socketpair: ', $!;
	unless (xfork) {
	    close S1;
	    open STDIN, '>&', \*S2 or die 'Cannot dup S2 ', $!;
	    open STDOUT, '>&', \*S2 or die 'Cannot dup S2 ', $!;
	    close S2;
	    exec './rx11ssh', @ARGV;
	    die 'exec: ', $!;
	}
	close S2;
	my $fd1 = fileno STDIN;
	my $fd2 = fileno S1;
	my $rin = '';
	vec($rin, $fd1, 1) = 1;
	vec($rin, $fd2, 1) = 1;
	while (1) {
	    usleep(1e5 + int rand 1e5);
	    my $rout;
	    my $x = select $rout = $rin, undef, undef, undef;
	    if (vec($rout, $fd1, 1) == 1) {
		copyio *STDIN, *S1 or die ' ';
	    }
	    if (vec($rout, $fd2, 1) == 1) {
		copyio *S1, *STDOUT or die ' ';
	    }
	}
	exit 0;
    }
    die "'@ARGV': unregognized command line\n";
}

die "'$clisock' exists. remove and try again if stale\n" if -e $clisock;
undef $clisock;

die 'compile rx11ssh' unless -x 'rx11ssh';

my $sockfile = '/tmp/.X11-unix/X776';

socket(SS, AF_UNIX, SOCK_STREAM, 0) or die 'socket: ', $!;
my $sockaddr = sockaddr_un($sockfile);

die "'$sockfile' exists. remove and try again if stale\n" if -e $sockfile;
my $mpid = $$;

END { unlink $sockfile if defined $mpid and $mpid == $$; }

bind(SS, $sockaddr) or die 'bind: ', $!;
listen(SS, 5) or die 'listen: ', $!;

unless (xfork) {
    # särver

    eval 'END { kill 15, $mpid if defined $mpid }';

    while (1) {
	unless (accept(S, SS)) {
	    next if $!{EINTR};
	    die 'accept: ', $!;
	}

	unless (xfork) {
	    undef $mpid;
	    close SS;
	    while (1) {
		copyio *S, *S or die ' ';
	    }
	    exit(0);
	}
	close S;
    }
    exit 0;
}

close SS;

unless (xfork) {
    exec qw!./rx11ssh :767:776 --ssh-command ./test-rx11ssh.pl delayer!;
    die 'exec: ', $!;
}

syswrite STDERR, "\n\n\n\n\n\n\n\n\n\n";

while (1) {
    usleep(1e5 + int rand 1e5);
    unless (xfork) {
	exec qw!./test-rx11ssh.pl xkilent!;
	die 'exec: ', $!;
    }
}
