# Makefile for src/mod/ctcp.mod/

srcdir = .


doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../ctcp.o

../ctcp.o:
	@echo -e "Compiling: \033[1mctcp\033[0m"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -DMAKING_MODS -c $(srcdir)/ctcp.c
	@rm -f ../ctcp.o
	@mv ctcp.o ../

depleaf:
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLEAF -MM $(srcdir)/ctcp.c > .depend

dephub:
	$(CC) $(CFLAGS) $(CPPFLAGS) -DHUB -MM $(srcdir)/ctcp.c > .depend

clean:
	@rm -f .depend *.o *~
distclean: clean

#safety hash
../ctcp.o: .././ctcp.mod/ctcp.c ../ctcp.mod/ctcp.h \
 ../../../src/mod/module.h \
 ../../../src/main.h ../../../config.h ../../../src/conf.h \
 ../../../src/lang.h ../../../src/eggdrop.h ../../../src/flags.h \
 ../../../src/proto.h ../../../lush.h ../../../src/misc_file.h \
 ../../../src/cmdt.h ../../../src/tclegg.h ../../../src/tclhash.h \
 ../../../src/chan.h ../../../src/users.h ../../../src/compat/compat.h ../../../src/compat/inet_ntop.h  \
 ../../../src/compat/inet_aton.h ../../../src/compat/snprintf.h \
 ../../../src/compat/memset.h ../../../src/compat/memcpy.h \
 ../../../src/compat/strcasecmp.h ../../../src/compat/strftime.h \
 ../../../src/mod/modvals.h ../../../src/tandem.h \
 ../../../src/mod/server.mod/server.h
