include ../common.mk

###
### Variables for the 'install' phase
###
DATDIR:=$(DATDIR)/darwinxref
INCDIR:=$(INCDIR)/darwinbuild

SOURCES= \
	DBDataStore.c \
	DBPlugin.c \
	DBTclPlugin.c \
	cfutils.c \
	main.c

PLUGIN_SOURCES=$(wildcard plugins/*.c)
PLUGIN_OBJECTS=$(PLUGIN_SOURCES:.c=.so) $(wildcard plugins/*.tcl)

### todo: --with-plugin-path configuration
CFLAGS+= \
	-DDEFAULT_DB_FILE=\".build/xref.db\" \
	-DDEFAULT_PLUGIN_PATH=\"$(subst $(DESTDIR),,$(DATDIR))/plugins\"

### todo: --with-tcl-plugins configuration
TCL_CFLAGS=-DHAVE_TCL_PLUGINS=1
TCL_LDFLAGS=-ltcl

CFLAGS+=-g -Wall -Werror $(TCL_CFLAGS) -DVERSION=\""$(shell cat ../VERSION)"\" $(SQLITEINC)
LDFLAGS+=-framework CoreFoundation $(TCL_LDFLAGS)
PLUGIN_CFLAGS=$(CFLAGS) -I.
PLUGIN_LDFLAGS=$(LDFLAGS) -bundle -bundle_loader $(SYMROOT)/darwinxref

VERSION=$(shell uname -r | cut -f1 -d. )
ifeq ($(VERSION),7)
SQLITEINC = -I../sqlite
SQLITELIB = ../sqlite/sqlite3.o
else
SQLITEINC =
SQLITELIB = -lsqlite3
endif
#ifeq ($(VERSION),9)
#CFLAGS += -mmacosx-version-min=10.5
#endif

CC = cc
DSYMUTIL = dsymutil

all: \
	darwinxref \
	$(PLUGIN_SOURCES:c=so) \
	upgrade_plist \
	apple_plugins

install: all install_apple_plugins
	[ -d $(BINDIR) ] || \
		$(INSTALL) -d $(INSTALL_DIR_FLAGS) $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) $(SYMROOT)/darwinxref $(BINDIR)

	[ -d $(DATDIR)/plugins ] || \
		$(INSTALL) -d $(INSTALL_DIR_FLAGS) $(DATDIR)/plugins
	$(INSTALL) $(INSTALL_DOC_FLAGS) $(wildcard $(OBJROOT)/plugins/*.so) $(wildcard plugins/*.tcl) $(DATDIR)/plugins

	[ -d $(INCDIR) ] || \
		$(INSTALL) -d $(INSTALL_DIR_FLAGS) $(INCDIR)
	$(INSTALL) $(INSTALL_DOC_FLAGS) DBPlugin.h cfutils.h $(INCDIR)

uninstall:
	rm -f $(BINDIR)/darwinxref
	[ ! -d $(DATDIR) ] || ( cd $(DATDIR) && rm -f $(PLUGIN_OBJECTS) )
	-rmdir $(DATDIR)/plugins
	-rmdir $(DATDIR)
	
	rm -f $(INCDIR)/DBPlugin.h
	rm -f $(INCDIR)/cfutils.h
	-rmdir $(INCDIR)

clean:
	rm -f darwinxref
	rm -rf darwinxref.dSYM
	rm -f $(SOURCES:.c=.o)
	rm -f $(PLUGIN_SOURCES:.c=.so)
	rm -rf $(PLUGIN_SOURCES:.c=.so.dSYM)
	rm -f upgrade_plist
	rm -rf upgrade_plist.dSYM

darwinxref: $(SOURCES)
	@for CFILE in $(SOURCES); do \
		OFILE=$$(echo $$(basename $${CFILE}) | sed -e 's,\.[^.]*$$,.o,') ; \
		CMD="$(CC) $(CFLAGS) -c -o $(OBJROOT)/$${OFILE} $${CFILE}"; \
		echo $$CMD ; $$CMD || exit 1 ; \
	done
	$(CC) $(LDFLAGS) -o $(SYMROOT)/darwinxref \
		$(SQLITELIB) \
		$(foreach OFILE, $(SOURCES:.c=.o), $(OBJROOT)/$(notdir $(OFILE)))
	$(DSYMUTIL) --out $(SYMROOT)/darwinxref.dSYM $(SYMROOT)/darwinxref || true

plugins/register.so: plugins/register.c DBPlugin.h
	@mkdir -p $(OBJROOT)/plugins
	cc -o $(OBJROOT)/$@ $(PLUGIN_CFLAGS) $(PLUGIN_LDFLAGS) $(SQLITEINC) $(SQLITELIB) -lcrypto $<

plugins/%.so: plugins/%.c DBPlugin.h
	@mkdir -p $(OBJROOT)/plugins
	cc -o $(OBJROOT)/$@ $(PLUGIN_CFLAGS) $(PLUGIN_LDFLAGS) $<

plugins-darwinports/%.so: plugins-darwinports/%.c DBPlugin.h
	cc -o $(OBJROOT)/$@ $(PLUGIN_CFLAGS) $(PLUGIN_LDFLAGS) $<

apple_plugins:
	@if [ -d plugins-apple ]; then			\
		make -C plugins-apple;			\
	fi

install_apple_plugins:
	@if [ -d plugins-apple ]; then			\
		make -C plugins-apple install;		\
	fi

upgrade_plist: upgrade_plist.c cfutils.c
	cc -o $(OBJROOT)/$@ $(CFLAGS) $(LDFLAGS) $^
