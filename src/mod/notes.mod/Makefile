# Makefile for src/mod/notes.mod

srcdir = .
depcomp = /bin/sh ../../../autotools/depcomp


doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../notes.o

../notes.o:
	@echo -e "Compiling: \033[1mnotes\033[0m"
	@source='notes.c' object='$@' depfile='.deps/notes.Po' tmpdepfile='.deps/notes.TPo' depmode=gcc3 $(depcomp) \
	$(CC) $(CFLAGS) $(CPPFLAGS) -DMAKING_MODS -c $(srcdir)/notes.c
	@rm -f ../notes.o
	@mv notes.o ../

clean:
	@rm -f .depend *.o *~

distclean: clean

