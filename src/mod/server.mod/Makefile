# Makefile for src/mod/server.mod/

srcdir = .


doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../server.o

../server.o:
	@echo -e "Compiling: \033[1mserver\033[0m"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -DMAKING_MODS -c $(srcdir)/server.c
	@rm -f ../server.o
	@mv server.o ../

depleaf:
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLEAF -MM $(srcdir)/server.c > .depend

dephub:
	$(CC) $(CFLAGS) $(CPPFLAGS) -DHUB -MM $(srcdir)/server.c > .depend

clean:
	@rm -f .depend *.o *~
distclean: clean

#safety hash
../server.o: .././server.mod/server.c ../../../src/mod/module.h \
 ../../../src/main.h ../../../config.h ../../../src/conf.h \
 ../../../src/lang.h \
 ../../../src/eggdrop.h ../../../src/flags.h ../../../src/proto.h \
 ../../../lush.h ../../../src/misc_file.h ../../../src/cmdt.h \
 ../../../src/tclegg.h ../../../src/tclhash.h ../../../src/chan.h \
 ../../../src/users.h ../../../src/compat/compat.h ../../../src/compat/inet_ntop.h \
 ../../../src/compat/inet_aton.h ../../../src/compat/snprintf.h \
 ../../../src/compat/memset.h ../../../src/compat/memcpy.h \
 ../../../src/compat/strcasecmp.h ../../../src/compat/strftime.h \
 ../../../src/mod/modvals.h ../../../src/tandem.h \
 ../server.mod/server.h ../server.mod/servmsg.c \
 ../server.mod/cmdsserv.c ../server.mod/tclserv.c
