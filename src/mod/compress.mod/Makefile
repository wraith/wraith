# Makefile for src/mod/compress.mod/

srcdir = .


doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../compress.o

../compress.o:
	@echo -e "Compiling: \033[1mcompress\033[0m"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -DMAKING_MODS -c $(srcdir)/compress.c
	@rm -f ../compress.o
	@mv compress.o ../

depend:
	@$(CC) $(CFLAGS) $(CPPFLAGS) -MM $(srcdir)/compress.c > .depend

clean:
	@rm -f .depend *.o *~
distclean: clean

#safety hash
../compress.o: .././compress.mod/compress.c ../../../src/mod/module.h \
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
 ../../../src/mod/share.mod/share.h \
 ../compress.mod/compress.h ../compress.mod/tclcompress.c
