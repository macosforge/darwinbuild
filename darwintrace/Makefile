include ../common.mk

LIBS=$(shell if [ -e /usr/lib/libSystemStubs.a ]; then echo -lSystemStubs; fi)

###
### Variables for the 'install' phase
###
DATDIR:=$(DATDIR)/darwinbuild

all: darwintrace.dylib

VERSION=$(shell uname -r | cut -f1 -d. )
ifeq ($(VERSION), 8)
CFLAGS = -nostdlib
else
CFLAGS = -nodefaultlibs
endif

ARCHS=$(shell lipo -info /usr/lib/libSystem.dylib | cut -d : -f 3 | sed 's/ppc7400/ppc/' | awk '{ ORS=" "; for(i=1;i<=NF;i++) print "-arch", $$i}') 

darwintrace.dylib: darwintrace.c
	cc -o $(OBJROOT)/$@ \
		$(ARCHS) \
		-W -Wall -pedantic -std=c99 \
		-flat_namespace \
		-fno-common \
		$(CFLAGS) \
		-undefined suppress \
		-dynamiclib \
		$^ $(LIBS)
clean:
	rm -f darwintrace.dylib

install: all
	[ -d $(DATDIR) ] || \
		$(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) $(OBJROOT)/darwintrace.dylib $(DATDIR)

uninstall:
	rm -f $(DATDIR)/darwintrace.dylib
	-rmdir $(DATDIR)
