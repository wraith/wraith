# Makefile for src/mod/compress.mod/

srcdir = .
depcomp = /bin/sh ../../../autotools/depcomp

#This line is simply for configure to generate .deps/
OBJS = compress.o

include ./.deps/includes

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../compress.o

../compress.o:
	@echo -e "Compiling: \033[1mcompress\033[0m"
	@source='compress.c' object='$@' depfile='.deps/compress.Po' tmpdepfile='.deps/compress.TPo' depmode=gcc3 $(depcomp) \
	$(CC) $(CFLAGS) $(CPPFLAGS) -DMAKING_MODS -c $(srcdir)/compress.c
	@rm -f ../compress.o
	@mv compress.o ../

clean:
	@rm -f .depend *.o *~

distclean: clean

