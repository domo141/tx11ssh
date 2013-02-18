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
# Last modified: Sun 17 Feb 2013 15:32:46 EET too

use 5.8.1;
use strict;
use warnings;
use Socket;

$ENV{'PATH'} = '/sbin:/usr/sbin:/bin:/usr/bin';

if ($ARGV) {
    if ($ARGV[0] eq  "kilent") {
    }
    if ($ARGV[0] eq  "delayer") {
    }
}
# särver

my $sockfile;
END { unlink $sockfile if defined $sockfile; }

my $f = '/tmp/.X11-unix/X777';

die "'$f' exists. remove and try again if stale\n" if -e $f;

socket(SS, AF_UNIX, SOCK_STREAM, 0) or die 'socket';

$sockfile = $f;
undef $f;

my $sockaddr = sockaddr_un($sockfile);

bind(SS, $sockaddr) or die 'bind';
listen(SS, 5) or die 'listen';

sub xfork()
{
    my $pid = fork;
    die 'fork' unless defined $pid;
    return $pid;
}

unless (xfork) {

}

close SS;

unless (xfork) {
}
