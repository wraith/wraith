# Makefile for src/mod/channels.mod/

srcdir = .
depcomp = /bin/sh ../../../autotools/depcomp

#This line is simply for configure to generate .deps/
OBJS = channels.o

include ./.deps/includes

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../channels.o

../channels.o:
	@echo -e "Compiling: \033[1mchannels\033[0m"
	@source='channels.c' object='$@' depfile='.deps/channels.Po' tmpdepfile='.deps/channels.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $(srcdir)/channels.c
	@rm -f ../channels.o
	@mv channels.o ../


clean:
	@rm -f .depend *.o *~

distclean: clean

