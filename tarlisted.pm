#!/usr/bin/perl
# -*- mode: cperl; cperl-indent-level: 4 -*-
# $ tarlisted.pm $
#
# Author: Tomi Ollila -- too ät iki piste fi
#
#	Copyright (c) 2013 Tomi Ollila
#	    All rights reserved
#
# Created: Fri 06 Dec 2013 14:30:05 EET too
# Last modified: Fri 21 Feb 2014 22:39:27 +0200 too

# This is copy of tarlisted.pm from https://github.com/domo141/tarlisted
# commit: 331e812a0e8555f695bb3c71  blob: 55f4895cf324342be48fb938

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   1. Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#   4. The names of the authors may not be used to endorse or promote
#      products derived from this software without specific prior
#      written permission.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

# This file is supposed to be carried into one's code tree and not to
# be installed as system component; Therefore the standard perl module
# syntax is not used here.

# There are (at least) 3 ways to use this from other perl program:

# BEGIN { require tarlisted }  --  in case the program is always run
#	in the same directory as this file is located.
#	or just 'use tarlisted' !

# BEGIN { my $dn = $0; $dn =~ s|[^/]+$||; require $dn . 'tarlisted.pm' }
#	in case the program loading this file is located in the same
#	directory as this file (easy to customize to other relative dirs).

# Just include this file to the beginning of a separate program.

# Functions of this file supposed to be called from outside:
   sub tarlisted_open($@); # name following optional compression program & args
   sub tarlisted_close();
   sub tarlisted_cp($@);
   sub tarlisted_stat($); # use w/ funcs below, called as &tarlisted_func()...
   sub tarlisted_cpstr($$$$$$@); # string as file
   sub tarlisted_mkdir($$$$$@);
#  sub tarlisted_mklink($$$$$@); # sym & hard links, to be implemented
#  sub tarlisted_mknod($$$$$@); # makes fifos too, to be implemented

use 5.6.1;
use strict;
use warnings;
use bytes;

my $_tarlisted_pid = -1;


sub _tarlisted_pipetocmd(@)
{
    pipe PR, PW;
    $_tarlisted_pid = fork;
    die "fork() failed: $!\n" unless defined $_tarlisted_pid;
    unless ($_tarlisted_pid) {
	# child
	close PW;
	open STDOUT, '>&TARLISTED';
	open STDIN, '>&PR';
	close PR;
	close TARLISTED;
	exec @_;
	die "exec() failed: $!";
    }
    # parent
    close PR;
    open TARLISTED, '>&PW';
    close PW;
}


# IEEE Std 1003.1-1988 (“POSIX.1”) ustar format
sub _tarlisted_mkhdr($$$$$$$$$$)
{
    if (length($_[7]) > 99) {
	die "Link name '$_[7]' too long\n";
    }
    my $name = $_[0];
    my $prefix;
    if (length($name) > 99) {
	die "Name splitting unimplemented ('$name' too long)\n";
    }
    else {
	$name = pack('a100', $name);
	$prefix = pack('a155', '');
    }
    my $mode = sprintf("%07o\0", $_[1]);
    my $uid = sprintf("%07o\0", $_[2]);
    my $gid = sprintf("%07o\0", $_[3]);
    my $size = sprintf("%011o\0", $_[4]);
    my $mtime = sprintf("%011o\0", $_[5]);
    my $checksum = '        ';
    my $typeflag = $_[6];
    my $linkname = pack('a100', $_[7]);
    my $magic = "ustar\0";
    my $version = '00';
    my $uname = pack('a32', $_[8]);
    my $gname = pack('a32', $_[9]);
    my $devmajor = "0000000\0";
    my $devminor = "0000000\0";
    my $pad = pack('a12', '');

    my $hdr = join '', $name, $mode, $uid, $gid, $size, $mtime,
      $checksum, $typeflag, $linkname, $magic, $version, $uname, $gname,
	$devmajor, $devminor, $prefix, $pad;

    my $sum = 0;
    foreach (split //, $hdr) {
	$sum = $sum + ord $_;
    }
    $checksum = sprintf "%06o\0 ", $sum;
    $hdr = join '', $name, $mode, $uid, $gid, $size, $mtime,
      $checksum, $typeflag, $linkname, $magic, $version, $uname, $gname,
	$devmajor, $devminor, $prefix, $pad;

    return $hdr;
}


sub _tarlisted_xsyswrite($)
{
    my $len = syswrite TARLISTED, $_[0] or 0;
    my $l = length $_[0];
    while ($len < $l) {
	die "Short write!\n" if $len <= 0;
	my $nl = syswrite TARLISTED, $_[0], $l - $len, $len or 0;
	die "Short write!\n" if $nl <= 0;
	$len += $nl;
    }
}

my $_tarlisted_wb = 0;


sub _tarlisted_writehdr($)
{
    _tarlisted_xsyswrite $_[0];
    $_tarlisted_wb += 512;
}


sub _tarlisted_addpad()
{
    if ($_tarlisted_wb % 512 != 0) {
	my $more = 512 - $_tarlisted_wb % 512;
	_tarlisted_xsyswrite "\0" x $more;
	$_tarlisted_wb += $more;
    }
}


# file [name perm mtime uid gid uname gname]
sub tarlisted_cp($@)
{
    my $name = $_[0];
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	$atime,$mtime,$ctime,$blksize,$blocks) = stat($name);

    die "stat failed: $!\n" unless defined $blocks;
    $name = $_[1] if defined $_[1];
    $mode = defined $_[2]? $_[2]: $mode & 0777;
    $mtime = $_[3] if defined $_[3];
    $uid = $_[4] if defined $_[4];
    $gid = $_[5] if defined $_[5];
    my $uname = defined $_[6]? $_[6]: getpwuid $uid;
    my $gname = defined $_[7]? $_[7]: getgrgid $gid;

    my $hdr = _tarlisted_mkhdr($name, $mode, $uid, $gid, $size,
			       $mtime, '0', '', $uname, $gname);
    _tarlisted_writehdr $hdr;

    open TARLISTED_IN, '<', $_[0] or die "opening '$_[0]': $!\n";
    my $buf; my $tlen = 0;
    while ( (my $len = sysread(TARLISTED_IN, $buf, 65536)) > 0) {
	_tarlisted_xsyswrite $buf;
	$tlen += $len;
    }
    die "Short read ($tlen != $size)!\n" if $tlen != $size;
    close TARLISTED_IN;
    $_tarlisted_wb += $tlen;
    _tarlisted_addpad;
}


# stat hälper
sub tarlisted_stat($)
{
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	$atime,$mtime,$ctime,$blksize,$blocks) = stat $_[0];

    die "stat failed: $!\n" unless defined $blocks;

    return ( $mode & 0777, $mtime, $uid, $gid, getpwuid $uid, getgrgid $gid );
}

# string name perm mtime uid gid [uname gname]
sub tarlisted_cpstr($$$$$$@)
{
    my $uname = defined $_[6]? $_[6]: getpwuid $_[4];
    my $gname = defined $_[7]? $_[7]: getgrgid $_[5];
    my $len = length $_[0];
    my $hdr = _tarlisted_mkhdr($_[1], $_[2], $_[4], $_[5], $len,
			       $_[3], '0', '', $uname, $gname);
    _tarlisted_writehdr $hdr;
    _tarlisted_xsyswrite $_[0];
    $_tarlisted_wb += $len;
    _tarlisted_addpad;
}

# dir perm mtime uid gid [uname gname]
sub tarlisted_mkdir($$$$$@)
{
    my $uname = defined $_[5]? $_[5]: getpwuid $_[3];
    my $gname = defined $_[6]? $_[6]: getgrgid $_[4];

    my $hdr = _tarlisted_mkhdr($_[0], $_[1], $_[3], $_[4], 0,
			       $_[2], '5', '', $uname, $gname);
    _tarlisted_writehdr $hdr;
}


sub tarlisted_open($@)
{
    die "tarlisted alreadly open\n" if $_tarlisted_pid >= 0;
    $_tarlisted_pid = 0;
    if ($_[0] eq '-') {
	open TARLISTED, '>&STDOUT' or die "dup stdout: $!\n";
	return;
    }
    open TARLISTED, '>', $_[0] or die "> $_[0]: $!\n";
    shift;
    _tarlisted_pipetocmd @_ if @_;
    $_tarlisted_wb = 0;
}


sub tarlisted_close()
{
    # end archive
    _tarlisted_xsyswrite "\0" x 1024;
    $_tarlisted_wb += 1024;

    if ($_tarlisted_wb % 10240 != 0) {
	my $more = 10240 - $_tarlisted_wb % 10240;
	_tarlisted_xsyswrite "\0" x $more;
	$_tarlisted_wb += $more;
    }
    close TARLISTED;
    waitpid $_tarlisted_pid, 0 if $_tarlisted_pid;
    $_tarlisted_pid = -1;
    return $?;
}

1;
