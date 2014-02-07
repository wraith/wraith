depcomp = /bin/sh $(top_srcdir)/build/autotools/depcomp

%.o: %.cpp $(top_srcdir)/src/stringfix
	@echo -e "[CC]	\033[1m$*\033[0m"
	trap "rm -f '.deps/$*.Tpo' $<.ii; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $<.ii; then \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $<.ii; \
	fi; \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $<.ii -o $@; \
	rm -f '.deps/$*.Tpo' $<.ii

%.o: %.c $(top_srcdir)/src/stringfix
	@echo -e "[C ]	\033[1m$*\033[0m"
	trap "rm -f '.deps/$*.Tpo' $<.i; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $<.i; then \
	    mv '.deps/$*.TPo' '.deps/$*.Po'; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CXXFLAGS) $(CPPFLAGS) -E $< | $(top_srcdir)/src/stringfix > $<.i; \
	fi; \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $<.i -o $@; \
	rm -f '.deps/$*.Tpo' $<.i
