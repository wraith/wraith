depcomp = /bin/sh $(top_srcdir)/build/autotools/depcomp

.SUFFIXES:
.SUFFIXES: .c .cc .h .o .So

.cc.So:
	@echo -e "{CC}	\033[1m$*\033[0m"
	set -e; trap "rm -f '.deps/$*.TPo' $*.ii; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $*.ii; then \
	    echo '$@: $(top_srcdir)/src/stringfix' >> '.deps/$*.TPo'; \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	   else \
	     rm -f '.deps/$*.TPo' $*.ii; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $*.ii; \
	fi; \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $*.ii -o $@; \
	rm -f $*.ii

.c.So:
	@echo -e "{C }	\033[1m$*\033[0m"
	set -e; trap "rm -f '.deps/$*.TPo' $*.i; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $*.i; then \
	    echo '$@: $(top_srcdir)/src/stringfix' >> '.deps/$*.TPo'; \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	   else \
	     rm -f '.deps/$*.TPo' $*.i; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $*.i; \
	fi; \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $*.i -o $@; \
	rm -f $*.i

.cc.o:
	@echo -e "[CC]	\033[1m$*\033[0m"
	set -e; trap "rm -f '.deps/$*.TPo'; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@; then \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	   else \
	     rm -f '.deps/$*.TPo'; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@; \
	fi

.c.o:
	@echo -e "[C]	\033[1m$*\033[0m"
	set -e; trap "rm -f '.deps/$*.TPo'; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@; then \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	   else \
	     rm -f '.deps/$*.TPo'; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@; \
	fi
