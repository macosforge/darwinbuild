include ../common.mk

###
### Variables for the 'install' phase
###
DATDIR:=$(DATDIR)/darwinbuild

PrefixReplacements = darwinbuild \
		     darwinmaster.sh \
		     packageRoots.sh \
		     thinPackages.sh \
		     installXcode

all: manifest prefixes

manifest: manifest.c
	cc $(CFLAGS) -Wall -Werror -o $(OBJROOT)/$@ -lcrypto $^

prefixes:
	@$(foreach SCRIPT,$(PrefixReplacements), \
		$(SED) -e 's,%%PREFIX%%,$(PREFIX),' $(SCRIPT).in > $(SCRIPT) ;)

install: all
	[ -d $(BINDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) darwinbuild $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) darwinmaster.sh $(BINDIR)/darwinmaster

	[ -d $(DATDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) darwinbuild.common $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode2 $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode3 $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode31 $(DATDIR)

	$(INSTALL) $(INSTALL_EXE_FLAGS) createChroot $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) $(OBJROOT)/manifest $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) ditto.sh $(DATDIR)/ditto
	$(INSTALL) $(INSTALL_EXE_FLAGS) buildlist.sh $(DATDIR)/buildlist
	$(INSTALL) $(INSTALL_EXE_FLAGS) buildorder.pl $(DATDIR)/buildorder
	$(INSTALL) $(INSTALL_EXE_FLAGS) packageRoots.sh $(DATDIR)/packageRoots
	$(INSTALL) $(INSTALL_EXE_FLAGS) synthfat.sh $(DATDIR)/synthfat
	$(INSTALL) $(INSTALL_EXE_FLAGS) thinFile.sh $(DATDIR)/thinFile
	$(INSTALL) $(INSTALL_EXE_FLAGS) thinPackages.sh $(DATDIR)/thinPackages

uninstall:
	rm -f $(BINDIR)/darwinbuild
	rm -f $(BINDIR)/darwinmaster
	rm -f $(DATDIR)/darwinbuild.common
	rm -f $(DATDIR)/installXcode
	rm -f $(DATDIR)/installXcode2
	rm -f $(DATDIR)/installXcode3
	rm -f $(DATDIR)/installXcode31
	rm -f $(DATDIR)/createChroot
	rm -f $(DATDIR)/manifest
	rm -f $(DATDIR)/ditto
	rm -f $(DATDIR)/buildlist
	rm -f $(DATDIR)/buildorder
	rm -f $(DATDIR)/packageRoots
	rm -f $(DATDIR)/synthfat
	rm -f $(DATDIR)/thinFile
	rm -f $(DATDIR)/thinPackages
	

clean:
	rm -f manifest
	@$(foreach SCRIPT,$(PrefixReplacements), \
		rm -f $(SCRIPT) ; )
