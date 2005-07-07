###
### Variables for the 'install' phase
###
PREFIX=/usr/local
DATDIR=$(PREFIX)/share/darwinbuild
INSTALL=install
INSTALL_EXE_FLAGS=-m 0755 -o root -g wheel
INSTALL_DIR_FLAGS=$(INSTALL_EXE_FLAGS)
INSTALL_DOC_FLAGS=-m 0644 -o root -g wheel

all: darwintrace.dylib

darwintrace.dylib: darwintrace.c
	cc -o $@ \
		-flat_namespace \
		-fno-common \
		-nostdlib \
		-undefined suppress \
		-dynamiclib \
		$^
clean:
	rm -f darwintrace.dylib

install: all
	[ -d $(DATDIR) ] || \
		$(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) darwintrace.dylib $(DATDIR)

uninstall:
	rm -f $(DATDIR)/darwintrace.dylib
	rmdir $(DATDIR)
