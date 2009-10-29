include ../common.mk

###
### Variables for the 'install' phase
###
DATDIR:=$(DATDIR)/darwinbuild

PrefixReplacements = darwinbuild \
		     darwinmaster.sh \
		     packageRoots.sh \
		     thinPackages.sh \
		     installXcode \
		     installXcode32

all: manifest prefixes

manifest: manifest.c
	cc $(CFLAGS) -Wall -Werror -o $(OBJROOT)/$@ -lcrypto $^

prefixes: $(PrefixReplacements)

$(PrefixReplacements): % : %.in
	$(SED) -e 's,%%PREFIX%%,$(PREFIX),' $@.in > $@ ;			

install: all
	[ -d $(BINDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) darwinbuild $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) darwinmaster $(BINDIR)

	[ -d $(DATDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) darwinbuild.common $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode2 $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode3 $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode31 $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode32 $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) Info.plist $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) SDKSettings.plist $(DATDIR)

	$(INSTALL) $(INSTALL_EXE_FLAGS) createChroot $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) $(OBJROOT)/manifest $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) ditto $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) buildlist $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) buildorder $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) packageRoots $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) synthfat $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) thinFile $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) thinPackages $(DATDIR)

uninstall:
	rm -f $(BINDIR)/darwinbuild
	rm -f $(BINDIR)/darwinmaster
	rm -f $(DATDIR)/darwinbuild.common
	rm -f $(DATDIR)/installXcode
	rm -f $(DATDIR)/installXcode2
	rm -f $(DATDIR)/installXcode3
	rm -f $(DATDIR)/installXcode31
	rm -f $(DATDIR)/installXcode32
	rm -f $(DATDIR)/Info.plist
	rm -f $(DATDIR)/SDKSettings.plist
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
