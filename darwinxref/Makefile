SOURCES= \
	DBDataStore.c \
	DBPlugin.c \
	DBTclPlugin.c \
	cfutils.c \
	main.c

PLUGIN_SOURCES=$(wildcard plugins/*.c) $(wildcard plugins-darwinports/*.c)
PLUGIN_OBJECTS=$(wildcard plugins/*.so) $(wildcard plugins/*.tcl)

### todo: --with-plugin-path configuration
CFLAGS+=-DDEFAULT_PLUGIN_PATH=\"$(DATDIR)/plugins\"

### todo: --with-tcl-plugins configuration
TCL_CFLAGS=-DHAVE_TCL_PLUGINS=1
TCL_LDFLAGS=-ltcl

CFLAGS+=-ggdb $(TCL_CFLAGS)
LDFLAGS+=-framework CoreFoundation $(TCL_LDFLAGS)
PLUGIN_CFLAGS=$(CFLAGS) -I.
PLUGIN_LDFLAGS=$(LDFLAGS) -bundle -bundle_loader darwinxref

###
### Variables for the 'install' phase
###
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
DATDIR=$(PREFIX)/share/darwinxref
INSTALL=install
INSTALL_EXE_FLAGS=-m 0755 -o root -g wheel
INSTALL_DIR_FLAGS=$(INSTALL_EXE_FLAGS)
INSTALL_DOC_FLAGS=-m 0644 -o root -g wheel

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
	cc -o $@ $(CFLAGS) $(LDFLAGS) -lsqlite3 $(SOURCES)

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
