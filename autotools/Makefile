
all: clean autoconf autoheader clean_again

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

autoheader:
	@rm -rf ../config.h.in
	@echo "Autoheader..."
	@autoheader
	@mv -f config.h.in ../config.h.in
	@ls -al ../config.h.in

clean:
	@echo "Cleaning..."
	@rm -rf autom4te.cache/ configure config.h.in

clean_again:
	@echo "Cleaning..."
	@rm -rf autom4te.cache/ configure config.h.in
