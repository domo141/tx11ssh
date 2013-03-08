# -*- makefile -*-

#SW = tx11ssh
VERSION = 0.99


CC = gcc

SHELL = /bin/sh

all: tx11ssh

tx11ssh: force # to make sure we have 'production' version
	sh tx11ssh.c CC=$(CC) p

tx11ssh-prod: tx11ssh.c
	sh tx11ssh.c CC=$(CC) p
	mv tx11ssh $@

install: tx11ssh-prod verchk
	@case "$(PREFIX)" in '') \
		echo Usage: make install PREFIX=prefix; exit 1; esac
	sed '1,/^$@.sh:/d;/^#.#eos/q' Makefile | sh -s "$(PREFIX)"

install.sh:
	test -n "$1" || exit 1 # internal shell script; not to be made directly
	set -eu
	PREFIX=$1
	die () { echo "$@" >&1; exit 1; }
	test -d "$PREFIX" || die "'$PREFIX': base directory missing."
	for d in bin share share/man share/man/man1
	do test -d "$PREFIX"/$d || mkdir "$PREFIX"/$d
	done
	set -x
	cp tx11ssh-prod "$PREFIX"/bin/tx11ssh
	cp tx11ssh.1 "$PREFIX"/share/man/man1/
#	#eos
	exit 1 # not reached

F = tx11ssh.c tx11ssh.1 Makefile TECHNOTES usock-buffer-test.c test-tx11ssh.pl\
	wrapx11usock.sh ldpreload_wrapx11usock.c

txz:	verchk
	tar --xform s,^,tx11ssh-$(VERSION)/, -Jcvf tx11ssh-$(VERSION).tar.xz $F

verchk:	force
	sed '1,/^verchk.sh:/d;/^#.#eos/q' Makefile | sh -s "$(VERSION)"

verchk.sh:
	test -n "$1" || exit 1 # internal shell script; not to be made directly
	set -eu
	die () { echo "$@" >&1; exit 1; }
	man_ver=`sed -n 's/^[.]TH[^"]*[^ ]* \([^"]*\).*/\1/p' tx11ssh.1`
	case $man_ver in "$1") ;; *)
		die "Manual page version '$man_ver' not '$(VERSION)'"
	esac
#	#eos
	exit 1 # not reached

d: force
	sh tx11ssh.c CC=$(CC) d

itest: d # infinite test
	./test-tx11ssh.pl

ltest1: d # local test
	./tx11ssh + --ssh-command ./tx11ssh none

ltest2: d # local test using ssh
	./tx11ssh - 127.1

ltest3: d # local test using preload lib
	sh ldpreload_wrapx11usock.c
	./tx11ssh - --ssh-command ./wrapx11usock.sh ./tx11ssh none

usbtest: usock-buffer-test
	./usock-buffer-test 4120

usock-buffer-test: usock-buffer-test
	sh $< CC=$(CC)


clean distclean: force
	rm -f tx11ssh tx11ssh-prod *.so usock-buffer-test *~

.PHONY: force

.SUFFIXES:
