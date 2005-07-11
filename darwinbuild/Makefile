###
### Variables for the 'install' phase
###
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
DATDIR=$(PREFIX)/share/darwinbuild
INSTALL=install
INSTALL_EXE_FLAGS=-m 0755 -o root -g wheel
INSTALL_DIR_FLAGS=$(INSTALL_EXE_FLAGS)
INSTALL_DOC_FLAGS=-m 0644 -o root -g wheel

all: 

install: all
	[ -d $(BINDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) darwinbuild $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcodebuild $(BINDIR)

	[ -d $(DATDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) darwinbuild.common $(DATDIR)

uninstall:
	rm -f $(BINDIR)/darwinbuild
	rm -f $(BINDIR)/installXcodebuild
	rm -f $(DATDIR)/darwinbuild.common
	rmdir $(DATDIR)
