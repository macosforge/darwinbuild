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
	$(INSTALL) $(INSTALL_EXE_FLAGS) darwinmaster.sh $(BINDIR)/darwinmaster

	[ -d $(DATDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) darwinbuild.common $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcodebuild $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) manifest $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) ditto.sh $(DATDIR)/ditto
	$(INSTALL) $(INSTALL_EXE_FLAGS) packageRoots.sh $(DATDIR)/packageRoots
	$(INSTALL) $(INSTALL_EXE_FLAGS) thinFile.sh $(DATDIR)/thinFile
	$(INSTALL) $(INSTALL_EXE_FLAGS) thinPackages.sh $(DATDIR)/thinPackages

uninstall:
	rm -f $(BINDIR)/darwinbuild
	rm -f $(DATDIR)/darwinbuild.common
	rm -f $(DATDIR)/installXcodebuild
	rm -f $(DATDIR)/manifest
	-rmdir $(DATDIR)

clean:
	rm -f manifest
