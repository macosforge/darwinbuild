include ../common.mk

###
### Variables for the 'install' phase
###
DATDIR:=$(DATDIR)/darwinbuild

override OBJROOT:=$(OBJROOT)/darwinbuild~obj

PrefixReplacements = darwinbuild \
		     darwinmaster.sh \
		     packageRoots.sh \
		     thinPackages.sh \
		     installXcode \
		     installXcode32

all: manifest prefixes

manifest: manifest.c
	@mkdir -p $(OBJROOT)
	cc $(CFLAGS) -Wall -Werror -o $(OBJROOT)/$@ -lcrypto $^

prefixes: $(PrefixReplacements)

$(PrefixReplacements): % : %.in
	@mkdir -p $(OBJROOT)
	$(SED) -e 's,%%PREFIX%%,$(PREFIX),' $@.in > $(OBJROOT)/$@ ;

install: all
	[ -d $(BINDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) $(OBJROOT)/darwinbuild $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) $(OBJROOT)/darwinmaster.sh $(BINDIR)/darwinmaster

	[ -d $(DATDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) darwinbuild.common $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) $(OBJROOT)/installXcode $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode2 $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode3 $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) installXcode31 $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) $(OBJROOT)/installXcode32 $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) Info.plist $(DATDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) SDKSettings.plist $(DATDIR)

	$(INSTALL) $(INSTALL_EXE_FLAGS) createChroot $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) $(OBJROOT)/manifest $(DATDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) ditto.sh $(DATDIR)/ditto
	$(INSTALL) $(INSTALL_EXE_FLAGS) buildlist.sh $(DATDIR)/buildlist
	$(INSTALL) $(INSTALL_EXE_FLAGS) buildorder.pl $(DATDIR)/buildorder
	$(INSTALL) $(INSTALL_EXE_FLAGS) $(OBJROOT)/packageRoots.sh $(DATDIR)/packageRoots
	$(INSTALL) $(INSTALL_EXE_FLAGS) synthfat.sh $(DATDIR)/synthfat
	$(INSTALL) $(INSTALL_EXE_FLAGS) thinFile.sh $(DATDIR)/thinFile
	$(INSTALL) $(INSTALL_EXE_FLAGS) $(OBJROOT)/thinPackages.sh $(DATDIR)/thinPackages

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
	rm -rf $(OBJROOT) 

