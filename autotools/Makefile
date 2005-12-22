
all: clean aclocal autoheader autoconf clean_again


aclocal:
	@rm -rf aclocal.m4
	aclocal -I includes/

autoheader:
	@rm -rf ../config.h.in
	@echo "Autoheader..."
	@autoheader
	@echo "" >> config.h.in
	@echo "/* Defines for pack features */" >> config.h.in
	@(for define in `sed -n -e 's/^[\+|\-] \([^ ]*\) .*/\1/ p' ../pack/pack.cfg`; do \
	echo "#undef S_$$define" >> config.h.in; \
	done)
	@mv -f config.h.in ../config.h.in
	@ls -al ../config.h.in

autoconf: 
	@rm -rf ../configure
	@echo "Autoconf...."
	@autoconf
	@echo "Fixing configure for cache_file"
	@(if ! sed configure -e "s/^cache_file=\/dev\/null/cache_file=\.\/config.cache/" \
	-e "s/&& echo \"updating cache .cache_file\"//" > ../configure; then \
	cp configure ../configure; \
	rm configure; \
	fi)
	@chmod 700 ../configure
	@ls -al ../configure

clean:
	@echo "Cleaning..."
	@rm -rf autom4te.cache/ configure config.h.in aclocal.m4

clean_again:
	@echo "Cleaning..."
	@rm -rf autom4te.cache/ configure config.h.in aclocal.m4
