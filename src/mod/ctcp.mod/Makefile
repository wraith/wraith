# Makefile for src/mod/ctcp.mod/

srcdir = .

OBJS = ctcp.o

include ./.deps/includes
include $(top_srcdir)/build/build.mk

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: $(OBJS)

clean:
	@rm -f .depend *.o *~

distclean: clean
