# Makefile for src/mod/share.mod/

srcdir = .
depcomp = /bin/sh ../../../autotools/depcomp

#This line is simply for configure to generate .deps/
OBJS = share.o

include ./.deps/includes

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../share.o

../share.o:
	@echo -e "Compiling: \033[1mshare\033[0m"
	@source='share.c' object='$@' depfile='.deps/share.Po' tmpdepfile='.deps/share.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $(srcdir)/share.c
	@rm -f ../share.o
	@mv share.o ../

clean:
	@rm -f .depend *.o *~

distclean: clean

