# -*- makefile -*-

CC=gcc

SHELL = /bin/sh

all: rx11ssh

rx11ssh: force
	sh rx11ssh.c CC=$(CC) p

tstbin: force
	sh rx11ssh.c CC=$(CC) d

itest: tstbin # infinite test
	./test-rx11ssh.pl

ltest1: tstbin # local test
	./rx11ssh --ssh-command ./rx11ssh none

ltest2: tstbin # local test using ssh
	./rx11ssh 127.1

usbtest: usock-buffer-test
	./usock-buffer-test 4120

usock-buffer-test: usock-buffer-test
	sh $< CC=$(CC)


clean distclean: force
	rm -f rx11ssh usock-buffer-test *~

.PHONY: force

.SUFFIXES:
