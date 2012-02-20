# Makefile for src/mod/server.mod/

srcdir = .
depcomp = /bin/sh ../../../build/autotools/depcomp

#This line is simply for configure to generate .deps/
OBJS = server.o

include ./.deps/includes

doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../server.o

../server.o:
	@echo -e "Compiling: \033[1mserver\033[0m"
	source='server.c' object='$@' depfile='.deps/server.Po' tmpdepfile='.deps/server.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $(srcdir)/server.c
	@rm -f ../server.o
	@mv server.o ../

clean:
	@rm -f .depend *.o *~

distclean: clean
