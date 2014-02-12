depcomp = /bin/sh $(top_srcdir)/build/autotools/depcomp

.SUFFIXES:
.SUFFIXES: .c .cc .h .o .So

.cc.So:
	@echo -e "{CC}	\033[1m$*\033[0m"
	trap "rm -f '.deps/$*.Tpo' $*.ii; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $*.ii; then \
	    echo '$@: $(top_srcdir)/src/stringfix' >> '.deps/$*.TPo'; \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $*.ii; \
	fi; \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $*.ii -o $@; \
	rm -f '.deps/$*.Tpo' $*.ii

.c.So:
	@echo -e "{C }	\033[1m$*\033[0m"
	trap "rm -f '.deps/$*.Tpo' $*.i; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $*.i; then \
	    echo '$@: $(top_srcdir)/src/stringfix' >> '.deps/$*.TPo'; \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $*.i; \
	fi; \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $*.i -o $@; \
	rm -f '.deps/$*.Tpo' $*.i

.cc.o:
	@echo -e "[CC]	\033[1m$*\033[0m"
	trap "rm -f '.deps/$*.Tpo'; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@; then \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@; \
	fi; \
	rm -f '.deps/$*.Tpo'

.c.o:
	@echo -e "[C]	\033[1m$*\033[0m"
	trap "rm -f '.deps/$*.Tpo'; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@; then \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@; \
	fi; \
	rm -f '.deps/$*.Tpo'
