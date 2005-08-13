include ../common.mk

LIBS=$(shell if [ -e /usr/lib/libSystemStubs.a ]; then echo -lSystemStubs; fi)

###
### Variables for the 'install' phase
###
DATDIR:=$(DATDIR)/darwinbuild

all: darwintrace.dylib

darwintrace.dylib: darwintrace.c
	cc -o $@ \
		-W -Wall -pedantic \
		-flat_namespace \
		-fno-common \
		-nostdlib \
		-undefined suppress \
		-dynamiclib \
		$^ $(LIBS)
clean:
	rm -f darwintrace.dylib

install: all
	[ -d $(DATDIR) ] || \
		$(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) darwintrace.dylib $(DATDIR)

uninstall:
	rm -f $(DATDIR)/darwintrace.dylib
	-rmdir $(DATDIR)
