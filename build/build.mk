depcomp = /bin/sh $(top_srcdir)/build/autotools/depcomp

.SUFFIXES:
.SUFFIXES: .c .cc .h .o .So

.cc.So:
	@echo -e "{CC}	\033[1m$*\033[0m"
	set -e; trap "rm -f '.deps/$*.TPo' $*.ii $*.fail; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if { { $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -E $< || :> $*.fail; } | \
	    $(top_srcdir)/src/stringfix > $*.ii; } && ! [ -f $*.fail ]; then \
	    echo '$@: $(top_srcdir)/src/stringfix' >> '.deps/$*.TPo'; \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	   else \
	     rm -f '.deps/$*.TPo' $*.ii $*.fail; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CPPFLAGS) $(CXXFLAGS) -E $< | $(top_srcdir)/src/stringfix > $*.ii; \
	fi; \
	$(CXX) $(CXXFLAGS) -c $*.ii -o $@; \
	rm -f $*.ii

.c.So:
	@echo -e "{C }	\033[1m$*\033[0m"
	set -e; trap "rm -f '.deps/$*.TPo' $*.i $*.fail; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if { { $(CC) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CPPFLAGS) $(CFLAGS) -E $< || :> $*.fail; } | \
	    $(top_srcdir)/src/stringfix > $*.i; } && ! [ -f $*.fail ]; then \
	    echo '$@: $(top_srcdir)/src/stringfix' >> '.deps/$*.TPo'; \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	   else \
	     rm -f '.deps/$*.TPo' $*.i $*.fail; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CC) $(CPPFLAGS) $(CFLAGS) -E $< | $(top_srcdir)/src/stringfix > $*.i; \
	fi; \
	$(CC) $(CFLAGS) -c $*.i -o $@; \
	rm -f $*.i

.cc.o:
	@echo -e "[CC]	\033[1m$*\033[0m"
	set -e; trap "rm -f '.deps/$*.TPo'; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@; then \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	   else \
	     rm -f '.deps/$*.TPo'; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@; \
	fi

.c.o:
	@echo -e "[C]	\033[1m$*\033[0m"
	set -e; trap "rm -f '.deps/$*.TPo'; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CC) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CPPFLAGS) $(CFLAGS) -c $< -o $@; then \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	   else \
	     rm -f '.deps/$*.TPo'; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@; \
	fi
