include ../common.mk

###
### Variables for the 'install' phase
###
DATDIR:=$(DATDIR)/darwinxref

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
	-DDEFAULT_DB_FILE=\".xref.db\" \
	-DDEFAULT_PLUGIN_PATH=\"$(DATDIR)/plugins\"

### todo: --with-tcl-plugins configuration
TCL_CFLAGS=-DHAVE_TCL_PLUGINS=1
TCL_LDFLAGS=-ltcl

CFLAGS+=-ggdb $(TCL_CFLAGS) -DVERSION=\"$(shell cat ../VERSION)\"
LDFLAGS+=-framework CoreFoundation $(TCL_LDFLAGS)
PLUGIN_CFLAGS=$(CFLAGS) -I.
PLUGIN_LDFLAGS=$(LDFLAGS) -bundle -bundle_loader darwinxref

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

uninstall:
	rm -f $(BINDIR)/darwinxref
	rm -rf $(DATDIR)/plugins

clean:
	rm darwinxref
	rm $(wildcard plugins/*.so)

darwinxref: $(SOURCES)
	ranlib ../sqlite/libsqlite3.a 2>/dev/null
	cc -o $@ \
		$(CFLAGS) \
		$(LDFLAGS) \
		-I../sqlite \
		$(SOURCES) \
		../sqlite/libsqlite3.a 

plugins/%.so: plugins/%.c DBPlugin.h
	cc -o $@ $(PLUGIN_CFLAGS) $(PLUGIN_LDFLAGS) $^

plugins-darwinports/%.so: plugins-darwinports/%.c DBPlugin.h
	cc -o $@ $(PLUGIN_CFLAGS) $(PLUGIN_LDFLAGS) $^

apple_plugins:
	if [ -d plugins-apple ]; then			\
		make -C plugins-apple;			\
	fi

install_apple_plugins:
	if [ -d plugins-apple ]; then			\
		make -C plugins-apple install;		\
	fi

upgrade_plist: upgrade_plist.c cfutils.c
	cc -o $@ $(CFLAGS) $(LDFLAGS) $^
