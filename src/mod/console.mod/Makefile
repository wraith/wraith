# Makefile for src/mod/console.mod/

srcdir = .
depcomp = /bin/sh ../../../build/autotools/depcomp

#This line is simply for configure to generate .deps/
OBJS = console.o

include ./.deps/includes

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../console.o

../console.o:
	@echo -e "Compiling: \033[1mconsole\033[0m"
	source='console.c' object='$@' depfile='.deps/console.Po' tmpdepfile='.deps/console.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $(srcdir)/console.c
	@rm -f ../console.o
	@mv console.o ../

clean:
	@rm -f .depend *.o *~

distclean: clean
