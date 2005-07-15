include ../common.mk

###
### Variables for the 'install' phase
###
DATDIR:=$(DATDIR)/darwinbuild

all: manifest

manifest: manifest.c
	cc -o $@ -lcrypto $^

install: all
	[ -d $(BINDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) darwinbuild $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcodebuild $(BINDIR)

	[ -d $(DATDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) darwinbuild.common $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) manifest $(DATDIR)

uninstall:
	rm -f $(BINDIR)/darwinbuild
	rm -f $(BINDIR)/installXcodebuild
	rm -f $(DATDIR)/darwinbuild.common
	rmdir $(DATDIR)
