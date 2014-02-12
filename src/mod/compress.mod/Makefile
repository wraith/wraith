# Makefile for src/mod/compress.mod/

srcdir = .

OBJS = compress.So

include ./.deps/includes
include $(top_srcdir)/build/build.mk

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: $(OBJS)

clean:
	@rm -f .depend *.So *~

distclean: clean

