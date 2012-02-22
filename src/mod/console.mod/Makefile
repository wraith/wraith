# Makefile for src/mod/console.mod/

srcdir = .
depcomp = /bin/sh ../../../build/autotools/depcomp

#This line is simply for configure to generate .deps/
OBJS = console.o

include ./.deps/includes
include ../mod.mk

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../console.o

clean:
	@rm -f .depend *.o *~

distclean: clean
