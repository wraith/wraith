# Makefile for src/mod/transfer.mod/

srcdir = .
depcomp = /bin/sh ../../../autotools/depcomp

#This line is simply for configure to generate .deps/
OBJS = transfer.o

include ./.deps/includes

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../transfer.o

../transfer.o:
	@echo -e "Compiling: \033[1mtransfer\033[0m"
	@source='transfer.c' object='$@' depfile='.deps/transfer.Po' tmpdepfile='.deps/transfer.TPo' depmode=gcc3 $(depcomp) \
	$(CC) $(CFLAGS) $(CPPFLAGS) -DMAKING_MODS -c $(srcdir)/transfer.c
	@rm -f ../transfer.o
	@mv transfer.o ../

clean:
	@rm -f .depend *.o *~

distclean: clean

