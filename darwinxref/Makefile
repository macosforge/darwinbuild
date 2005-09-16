include ../common.mk

###
### Variables for the 'install' phase
###
DATDIR:=$(DATDIR)/darwinxref
INCDIR:=$(INCDIR)/DarwinBuild

SOURCES= \
	DBDataStore.c \
	DBPlugin.c \
	DBTclPlugin.c \
	cfutils.c \
	main.c

PLUGIN_SOURCES=$(wildcard plugins/*.c) $(wildcard plugins-darwinports/*.c)
PLUGIN_OBJECTS=$(wildcard plugins/*.so) $(wildcard plugins/*.tcl)

### todo: --with-plugin-path configuration
CFLAGS+= \
	-DDEFAULT_DB_FILE=\".build/xref.db\" \
	-DDEFAULT_PLUGIN_PATH=\"$(subst $(DESTDIR),,$(DATDIR))/plugins\"

### todo: --with-tcl-plugins configuration
TCL_CFLAGS=-DHAVE_TCL_PLUGINS=1
TCL_LDFLAGS=-ltcl

CFLAGS+=-g $(TCL_CFLAGS) -DVERSION=\"$(shell cat ../VERSION)\" $(SQLITEINC)
LDFLAGS+=-framework CoreFoundation $(TCL_LDFLAGS)
PLUGIN_CFLAGS=$(CFLAGS) -I.
PLUGIN_LDFLAGS=$(LDFLAGS) -bundle -bundle_loader darwinxref

VERSION=$(shell uname -r | cut -f1 -d. )
ifeq ($(VERSION), 7)
SQLITEINC = -I../sqlite
SQLITELIB = ../sqlite/sqlite3.o
else
SQLITEINC =
SQLITELIB = -lsqlite3
endif

all: \
	darwinxref \
	$(PLUGIN_SOURCES:c=so) \
	upgrade_plist \
	apple_plugins

install: all install_apple_plugins
	[ -d $(BINDIR) ] || \
		$(INSTALL) -d $(INSTALL_DIR_FLAGS) $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) darwinxref $(BINDIR)

	[ -d $(DATDIR)/plugins ] || \
		$(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)/plugins
	$(INSTALL) $(INSTALL_DOC_FLAGS) $(PLUGIN_OBJECTS) $(DATDIR)/plugins

	[ -d $(INCDIR) ] || \
		$(INSTALL) -d $(INSTALL_DIR_FLAGS) $(INCDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) DBPlugin.h cfutils.h $(INCDIR)

uninstall:
	rm -f $(BINDIR)/darwinxref
	[ ! -d $(DATDIR) ] || ( cd $(DATDIR) && rm -f $(PLUGIN_OBJECTS) )
	-rmdir $(DATDIR)

clean:
	rm -f darwinxref
	rm -f $(PLUGIN_SOURCES:c=so)
	rm -f upgrade_plist

darwinxref: $(SOURCES)
	cc -o $@ \
		$(CFLAGS) \
		$(LDFLAGS) \
		$(SOURCES) \
		$(SQLITELIB)

plugins/register.so: plugins/register.c DBPlugin.h
	cc -o $@ $(PLUGIN_CFLAGS) $(PLUGIN_LDFLAGS) -lcrypto $<

plugins/%.so: plugins/%.c DBPlugin.h
	cc -o $@ $(PLUGIN_CFLAGS) $(PLUGIN_LDFLAGS) $<

plugins-darwinports/%.so: plugins-darwinports/%.c DBPlugin.h
	cc -o $@ $(PLUGIN_CFLAGS) $(PLUGIN_LDFLAGS) $<

apple_plugins:
	@if [ -d plugins-apple ]; then			\
		make -C plugins-apple;			\
	fi

install_apple_plugins:
	@if [ -d plugins-apple ]; then			\
		make -C plugins-apple install;		\
	fi

upgrade_plist: upgrade_plist.c cfutils.c
	cc -o $@ $(CFLAGS) $(LDFLAGS) $^
