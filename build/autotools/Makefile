
all: clean aclocal autoheader autoconf clean_again


aclocal:
	@rm -rf aclocal.m4
	@mkdir src
	aclocal -I includes/

autoheader:
	@rm -f ../../src/config.h.in
	@echo "Autoheader..."
	@autoheader
	@mv -f src/config.h.in ../../src/config.h.in
	@ls -al ../../src/config.h.in

autoconf: 
	@rm -rf ../../configure
	@echo "Autoconf...."
	@autoconf
	@echo "Fixing configure temp paths"
	@(if ! sed configure \
	  -e 's:config\.log:build/config.log:g' \
	  -e 's:config\.status:build/config.status:g' \
	  -e 's:confdefs\.h:build/confdefs.h:g' \
	  > ../../configure; then \
	  echo "FAILED TO FIX configure" >&2; \
          cp configure ../../configure; \
          rm configure; \
	fi)
	@chmod 700 ../../configure
	@ls -al ../../configure

clean:
	@echo "Cleaning..."
	@rm -rf autom4te.cache/ configure aclocal.m4 src/

clean_again:
	@echo "Cleaning..."
	@rm -rf autom4te.cache/ configure aclocal.m4 src/
