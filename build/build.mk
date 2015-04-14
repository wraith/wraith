depcomp = /bin/sh $(top_srcdir)/build/autotools/depcomp

STRINGFIX= $(srcdir)/stringfix

$(STRINGFIX): $(STRINGFIX).cc
	@echo -e "[CXX]	\033[1m$@\033[0m"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDFLAGS) -o $(STRINGFIX)

# Cannot use .SUFFIXES as it won't allow a dependency on $(STRINGFIX)

%.So: %.cc $(STRINGFIX)
	@echo -e "{CXX}	\033[1m$<\033[0m"
	file="$*"; \
	dirname="$${file%/*}"; \
	if [ "$${dirname}" = "$${file}" ]; then dirname=.; fi; \
	file="$${file##*/}"; \
	deps="$${dirname}/.deps/$${file}"; \
	set -e; trap "rm -f "$${deps}.TPo" $*.ii $*.fail; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if { { $(CXX) -MT '$@' -MD -MP -MF "$${deps}.TPo" $(CXXFLAGS) $(CPPFLAGS) -E $< || :> $*.fail; } | \
	    $(STRINGFIX) > $*.ii; } && ! [ -f $*.fail ]; then \
	    echo '$@: $(STRINGFIX)' >> "$${deps}.TPo"; \
	    mv "$${deps}.TPo" "$${deps}.Po"; \
	   else \
	     rm -f "$${deps}.TPo" $*.ii $*.fail; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile="$${deps}.Po" tmpdepfile="$${deps}.TPo" depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CPPFLAGS) $(CXXFLAGS) -E $< | $(STRINGFIX) > $*.ii; \
	fi; \
	$(CXX) $(CXXFLAGS) -c $*.ii -o $@; \
	rm -f $*.ii

%.So: %.c $(STRINGFIX)
	@echo -e "{CC }	\033[1m$<\033[0m"
	file="$*"; \
	dirname="$${file%/*}"; \
	if [ "$${dirname}" = "$${file}" ]; then dirname=.; fi; \
	file="$${file##*/}"; \
	deps="$${dirname}/.deps/$${file}"; \
	set -e; trap "rm -f "$${deps}.TPo" $*.i $*.fail; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if { { $(CC) -MT '$@' -MD -MP -MF "$${deps}.TPo" $(CPPFLAGS) $(CFLAGS) -E $< || :> $*.fail; } | \
	    $(STRINGFIX) > $*.i; } && ! [ -f $*.fail ]; then \
	    echo '$@: $(STRINGFIX)' >> "$${deps}.TPo"; \
	    mv "$${deps}.TPo" "$${deps}.Po"; \
	   else \
	     rm -f "$${deps}.TPo" $*.i $*.fail; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile="$${deps}.Po" tmpdepfile="$${deps}.TPo" depmode=$(CCDEPMODE) $(depcomp) \
	  $(CC) $(CPPFLAGS) $(CFLAGS) -E $< | $(STRINGFIX) > $*.i; \
	fi; \
	$(CC) $(CFLAGS) -c $*.i -o $@; \
	rm -f $*.i

%.o: %.cc
	@echo -e "[CXX]	\033[1m$<\033[0m"
	file="$*"; \
	dirname="$${file%/*}"; \
	if [ "$${dirname}" = "$${file}" ]; then dirname=.; fi; \
	file="$${file##*/}"; \
	deps="$${dirname}/.deps/$${file}"; \
	set -e; trap "rm -f "$${deps}.TPo"; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CXX) -MT '$@' -MD -MP -MF "$${deps}.TPo" $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@; then \
	    mv "$${deps}.TPo" "$${deps}.Po"; \
	   else \
	     rm -f "$${deps}.TPo"; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile="$${deps}.Po" tmpdepfile="$${deps}.TPo" depmode=$(CCDEPMODE) $(depcomp) \
	  $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@; \
	fi

%.o: %.c
	@echo -e "[CC ]	\033[1m$<\033[0m"
	file="$*"; \
	dirname="$${file%/*}"; \
	if [ "$${dirname}" = "$${file}" ]; then dirname=.; fi; \
	file="$${file##*/}"; \
	deps="$${dirname}/.deps/$${file}"; \
	set -e; trap "rm -f "$${deps}.TPo"; exit 1" 1 2 3 5 10 13 15; \
	if [ "$(CCDEPMODE)" = "gcc3" ]; then \
	  if $(CC) -MT '$@' -MD -MP -MF "$${deps}.TPo" $(CPPFLAGS) $(CFLAGS) -c $< -o $@; then \
	    mv "$${deps}.TPo" "$${deps}.Po"; \
	   else \
	     rm -f "$${deps}.TPo"; \
	     exit 1; \
	  fi; \
	else \
	  libtool=no source='$<' object='$@' depfile="$${deps}.Po" tmpdepfile="$${deps}.TPo" depmode=$(CCDEPMODE) $(depcomp) \
	  $(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@; \
	fi
