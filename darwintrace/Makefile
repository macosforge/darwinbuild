include ../common.mk

LIBS=$(shell if [ -e /usr/lib/libSystemStubs.a ]; then echo -lSystemStubs; fi)

###
### Variables for the 'install' phase
###
DATDIR:=$(DATDIR)/darwinbuild

override OBJROOT:=$(OBJROOT)/darwintrace~obj

all: darwintrace.dylib

VERSION=$(shell uname -r | cut -f1 -d. )
ifeq ($(VERSION), 8)
CFLAGS += -nostdlib
else
CFLAGS += -nodefaultlibs
endif

darwintrace.dylib: darwintrace.c
	@mkdir -p $(OBJROOT)
	cc -o $(OBJROOT)/$@ \
		$(CFLAGS) \
		-Wall -Werror -pedantic -std=c99 \
		-flat_namespace \
		-fno-common \
		$(CFLAGS) \
		-undefined suppress \
		-dynamiclib \
		$^ $(LIBS)
clean:
	rm -rf $(OBJROOT) 

install: all
	[ -d $(DATDIR) ] || \
		$(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) $(OBJROOT)/darwintrace.dylib $(DATDIR)

uninstall:
	rm -f $(DATDIR)/darwintrace.dylib
	-rmdir $(DATDIR)
