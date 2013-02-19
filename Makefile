

CC=gcc

.PHONY: all
all: rx11ssh

rx11ssh: rx11ssh.c
	sh $< CC=$(CC) p

.PHONY: tstbin
tstbin: rx11ssh.c
	sh $< CC=$(CC) d

itest: tstbin # infinite test
	./test-rx11ssh.pl

ltest: tstbin # local test
	./rx11ssh --ssh-command ./rx11ssh none

usbtest:
	sh usock-buffer-test.c CC=$(CC)
	./usock-buffer-test 4120

.PHONY: clean
clean:
	rm -f rx11ssh usock-buffer-test

.SUFFIXES:
