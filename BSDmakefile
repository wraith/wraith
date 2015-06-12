#.PHONY: default check_gmake debug static dynamic clean distclean test

MAKE=env -u MAKELEVEL gmake ${MFLAGS}

TARGETS=	\
		all \
		debug \
		static \
		dynamic \
		clean \
		distclean \
		test \
		check

.for target in ${TARGETS}
${target}: check_gmake .PHONY .SILENT
	@${MAKE} ${.TARGET}
.endfor

check_gmake: .PHONY .SILENT
	@which gmake > /dev/null 2>&1 || (echo "Please install gmake" && exit 0)
