# Makefile for src/mod/update.mod/

srcdir = .
depcomp = /bin/sh ../../../autotools/depcomp

#This line is simply for configure to generate .deps/
OBJS = update.o

include ./.deps/includes

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../update.o

../update.o:
	@echo -e "Compiling: \033[1mupdate\033[0m"
	source='update.c' object='$@' depfile='.deps/update.Po' tmpdepfile='.deps/update.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $(srcdir)/update.c
	@rm -f ../update.o
	@mv update.o ../

clean:
	@rm -f .depend *.o *~

distclean: clean

