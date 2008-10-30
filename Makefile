Project	= darwinbuild
SubProjects = darwinbuild \
	darwintrace \
	darwinup \
	darwinxref 

Install_Flags = DESTDIR=$(DSTROOT)

SUBDIRS= \
	darwinbuild \
	darwinxref \
	darwinup \
	darwintrace

.PHONY: all clean install uninstall

all clean install uninstall:
	@$(foreach DIR,$(SUBDIRS), \
		(echo "*** Making $@ in $(DIR) ***" ; \
		make -C $(DIR) $@) || exit 1; )

installsrc:
	tar czf - . | tar xzf - -C "$(SRCROOT)" --exclude=.svn --exclude=sqlite --exclude=www --exclude=darwinup/libredo.o

installhdrs:
	@echo Nothing to be done for $@
