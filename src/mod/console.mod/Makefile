# Makefile for src/mod/console.mod/

srcdir = .


doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../console.o

../console.o:
	@echo -e "Compiling: \033[1mconsole\033[0m"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -DMAKING_MODS -c $(srcdir)/console.c
	@rm -f ../console.o
	@mv console.o ../

depend:
	@$(CC) $(CFLAGS) $(CPPFLAGS) -MM $(srcdir)/console.c > .depend

clean:
	@rm -f .depend *.o *~
distclean: clean

#safety hash
../console.o: .././console.mod/console.c ../../../src/mod/module.h \
 ../../../src/main.h ../../../config.h ../../../pack/conf.h \
 ../../../src/lang.h \
 ../../../src/eggdrop.h ../../../src/flags.h ../../../src/proto.h \
 ../../../lush.h ../../../src/misc_file.h ../../../src/cmdt.h \
 ../../../src/tclegg.h ../../../src/tclhash.h ../../../src/chan.h \
 ../../../src/users.h ../../../src/compat/compat.h ../../../src/compat/inet_ntop.h \
 ../../../src/compat/inet_aton.h ../../../src/compat/snprintf.h \
 ../../../src/compat/memset.h ../../../src/compat/memcpy.h \
 ../../../src/compat/strcasecmp.h ../../../src/compat/strftime.h \
 ../../../src/mod/modvals.h ../../../src/tandem.h \
 ../console.mod/console.h
