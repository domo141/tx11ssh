# -*- makefile -*-

VERSION = wip-1.1
DATE = 2013-11-01

CC = gcc

SHELL = /bin/sh

all: tx11ssh

# "release" and "devel" versions build to the same name so those
# can be used interchangeable in tests (when other end is compiled)
# on remote system.

tx11ssh: force # to make sure we have 'release' version
	sh tx11ssh.c CC=$(CC) p

tx11ssh-rel: tx11ssh.c
	sh tx11ssh.c CC=$(CC) p -DVERSION='"$(VERSION)"'
	mv tx11ssh $@

install: tx11ssh-rel verchk
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
	cp tx11ssh-rel "$PREFIX"/bin/tx11ssh
	cp tx11ssh.1 "$PREFIX"/share/man/man1/
#	#eos
	exit 1 # not reached

F = tx11ssh.c tx11ssh.1 Makefile usock-buffer-test.c test-tx11ssh.pl \
	README.md LICENSE TECHNOTES wrapx11usock.sh ldpreload_wrapx11usock.c

txz:	README.md
	tar --xform s,^,tx11ssh-$(VERSION)/, -Jcvf tx11ssh-$(VERSION).tar.xz $F

README.md: verchk
	sed '1,/^$@.sh:/d;/^#.#eos/q' Makefile | sh -s "$(VERSION)" "$@"

README.md.sh:
	test -n "$1" || exit 1 # internal shell script; not to be made directly
	set -eu
	case ${BASH_VERSION-} in *.*) shopt -s xpg_echo; esac
	exec > "$2"
	GROFF_NO_SGR=1 TERM=vt100; export GROFF_NO_SGR TERM
	echo "<!-- Generated by make $2 embedding tx11ssh.1 to the content -->"
	echo
	echo "# Tx11ssh $1"
	echo
	echo 'Tx11ssh provides a way to (forward or reverse) tunnel X11'
	echo 'traffic over ssh connection.'
	echo
	echo 'Read the embedded manual page below for more information.'
	echo '<br/><small>(Decide yourself which direction you consider being'
	echo '"reverse" and which "forward" tunneling ;)</small>'
	echo
	echo 'For techical info read [TECHNOTES](TECHNOTES).'
	echo
	echo '<pre>'
	groff -man -T latin1 tx11ssh.1 | perl -e '
	my %htmlqh = qw/& &amp;   < &lt;   > &gt;   '\'' &apos;   " &quot;/;
	sub htmlquote($) { $_[0] =~ s/([&<>'\''"])/$htmlqh{$1}/ge; }
	while (<>) { htmlquote $_; s/[_&]\010&/&/g;
		s/((?:_\010[^_])+)/<u>$1<\/u>/g; s/_\010(.)/$1/g;
		s/((?:.\010.)+)/<b>$1<\/b>/g; s/.\010(.)/$1/g; print $_; }'
	echo '</pre>'
	exec > /dev/null
#	#eos
	exit 1 # not reached

verchk:	force
	sed '1,/^verchk.sh:/d;/^#.#eos/q' Makefile | sh -s "$(VERSION)" $(DATE)

verchk.sh:
	test -n "$1" || exit 1 # internal shell script; not to be made directly
	set -eu
	die () { echo "$@" >&1; ev=1; }
	ev=0
	man_ver=`sed -n 's/^[.]TH[^"]*[^ ]* \([^"]*\).*/\1/p' tx11ssh.1`
	case $man_ver in "$1") ;; *)
		die "Manual page version '$man_ver' not '$1'."
	esac
	man_date=`awk '/^[.]TH /{print $4}' tx11ssh.1`
	case $man_date in "$2") ;; *)
		die "Manual page date '$man_date' not '$2'."
	esac
	exit $ev
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
	rm -f tx11ssh tx11ssh-rel *.so *.html usock-buffer-test *~

.PHONY: force

.SUFFIXES:
