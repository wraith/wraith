# Makefile for src/mod/ctcp.mod/

srcdir = .
depcomp = /bin/sh ../../../autotools/depcomp


doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../ctcp.o

../ctcp.o:
	@echo -e "Compiling: \033[1mctcp\033[0m"
	@source='ctcp.c' object='$@' depfile='.deps/ctcp.Po' tmpdepfile='.deps/ctcp.TPo' depmode=gcc3 $(depcomp) \
	$(CC) $(CFLAGS) $(CPPFLAGS) -DMAKING_MODS -c $(srcdir)/ctcp.c
	@rm -f ../ctcp.o
	@mv ctcp.o ../

clean:
	@rm -f .depend *.o *~

distclean: clean
