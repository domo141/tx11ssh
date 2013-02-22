# -*- makefile -*-

CC=gcc

SHELL = /bin/sh

all: rx11ssh

rx11ssh: force # to make sure we have 'production' version
	sh rx11ssh.c CC=$(CC) p

d: force
	sh rx11ssh.c CC=$(CC) d

itest: d # infinite test
	./test-rx11ssh.pl

ltest1: d # local test
	./rx11ssh --ssh-command ./rx11ssh none

ltest2: d # local test using ssh
	./rx11ssh 127.1

usbtest: usock-buffer-test
	./usock-buffer-test 4120

usock-buffer-test: usock-buffer-test
	sh $< CC=$(CC)


clean distclean: force
	rm -f rx11ssh usock-buffer-test *~

.PHONY: force

.SUFFIXES:
