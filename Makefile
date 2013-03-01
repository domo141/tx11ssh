# -*- makefile -*-

CC=gcc

SHELL = /bin/sh

all: tx11ssh

tx11ssh: force # to make sure we have 'production' version
	sh tx11ssh.c CC=$(CC) p

d: force
	sh tx11ssh.c CC=$(CC) d

itest: d # infinite test
	./test-tx11ssh.pl

ltest1: d # local test
	./tx11ssh + --ssh-command ./tx11ssh none

ltest2: d # local test using ssh
	./tx11ssh - 127.1

ltest3: d # local test using preload lib
	./tx11ssh + --ssh-command env LD_PRELOAD=./wrapx11usock.so ./tx11ssh

usbtest: usock-buffer-test
	./usock-buffer-test 4120

usock-buffer-test: usock-buffer-test
	sh $< CC=$(CC)


clean distclean: force
	rm -f tx11ssh usock-buffer-test *~

.PHONY: force

.SUFFIXES:
