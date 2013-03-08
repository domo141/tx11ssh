#!/bin/sh
# -*- text -*-

set -eu

libpath=./

DISPLAY=:11
archos=`uname -s -m | awk '{ print tolower($2) "-" tolower($1) }'`
LD_PRELOAD=${libpath}ldpreload_wrapx11usock-$archos.so
export DISPLAY LD_PRELOAD

exec "$@"
exit 1

Edit the path of `./wrapx11usock.so` in case this script and the preload
library are not located to $HOME directory.

If you want to use this preload library from suid binaries copy the library
to one of the paths configured in /etc/ld.so.c* and chmod 4444 the
library file. Then using LD_PRELOAD=wrapx11usock.so (i.e. without path
components) will make standard dynamic linked (at least glibc version)
load this library in the memory of suid binary (and then clean LD_PRELOAD
from environment). The suid binary will use the preload library, just that
without re-introducing the same LD_PRELOAD setting further execve(1)d
binaries from that suid binary will not use the same preload library anymore.
