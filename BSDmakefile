.PHONY: default check_gmake debug static dynamic clean distclean test

default: check_gmake
	@gmake
check_gmake:
	@which gmake > /dev/null 2>&1 || (echo "Please install gmake" && exit 0)
debug: check_gmake
	@gmake debug
static: check_gmake
	@gmake static
dynamic: check_gmake
	@gmake dynamic
clean: check_gmake
	@gmake clean
distclean: check_gmake
	@gmake distclean
test:
	@gmake test
