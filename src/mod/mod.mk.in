depcomp = /bin/sh $(top_srcdir)/build/autotools/depcomp

../%.o: %.c $(top_srcdir)/src/stringfix $(top_srcdir)/build/cc1plus
	@echo -e "Compiling: \033[1m$*\033[0m"
ifeq ($(CCDEPMODE),gcc3)
	if STRINGFIX='$(top_srcdir)/$(STRINGFIX)' $(CXX) -MT '$@' -MD -MP -MF '.deps/$*.TPo' -DSTRINGFIX=$(STRINGFIX) -B$(top_srcdir)/build $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@; then \
	mv '.deps/$*.TPo' '.deps/$*.Po'; \
	else rm -f '.deps/$*.Tpo'; exit 1; \
	fi
else
	# STRINGFIX included after CXX for ccache to recognize
	STRINGFIX="$(top_srcdir)/$(STRINGFIX)" libtool=no source='$<' object='$@' depfile='.deps/$*.Po' tmpdepfile='.deps/$*.TPo' depmode=$(CCDEPMODE) $(depcomp) \
	$(CXX) -DSTRINGFIX=$(STRINGFIX) -B$(top_srcdir)/build $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@
endif
