# Makefile for src/mod/irc.mod/

srcdir = .
depcomp = /bin/sh ../../../autotools/depcomp


#This line is simply for configure to generate .deps/
OBJS = irc.o

include ./.deps/includes

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../irc.o

../irc.o:
	@echo -e "Compiling: \033[1mirc\033[0m"
	source='irc.c' object='$@' depfile='.deps/irc.Po' tmpdepfile='.deps/irc.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $(srcdir)/irc.c
	@rm -f ../irc.o
	@mv irc.o ../


clean:
	@rm -f .depend *.o *~

distclean: clean

