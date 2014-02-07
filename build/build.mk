depcomp = /bin/sh $(top_srcdir)/build/autotools/depcomp

%.o: %.cpp $(top_srcdir)/src/stringfix
	@echo -e "[CC]	\033[1m$*\033[0m"
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $<.ii; then \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	  else \
	    rm -f '.deps/$*.Tpo'; \
	    exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $<.ii; \
	fi; \
	if ! $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $<.ii -o $@; then \
	  rm -f $<.ii; \
	  exit 1; \
	else \
	  rm -f $<.ii; \
	fi

%.o: %.c $(top_srcdir)/src/stringfix
	@echo -e "[C ]	\033[1m$*\033[0m"
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $<.i; then \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	  else \
	    rm -f '.deps/$*.Tpo'; \
	    exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $<.i; \
	fi; \
	if ! $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $<.i -o $@; then \
	  rm -f $<.i; \
	  exit 1; \
	else \
	  rm -f $<.i; \
	fi
