SOURCES= \
	DBDataStore.c \
	DBPlugin.c \
	cfutils.c \
	main.c

PLUGIN_SOURCES=$(wildcard plugins/*.c)
CFLAGS= -ggdb 
LDFLAGS=-framework CoreFoundation
PLUGIN_CFLAGS=$(CFLAGS) -I.
PLUGIN_LDFLAGS=$(LDFLAGS) -bundle -bundle_loader darwinxref

all: \
	darwinxref \
	$(PLUGIN_SOURCES:c=so) \
	upgrade_plist

clean:
	rm darwinxref
	rm $(wildcard plugins/*.so)

darwinxref: $(SOURCES)
	cc -o $@ $(CFLAGS) $(LDFLAGS) -lsqlite3 $(SOURCES)

plugins/%.so: plugins/%.c DBPlugin.h
	cc -o $@ $(PLUGIN_CFLAGS) $(PLUGIN_LDFLAGS) $^

upgrade_plist: upgrade_plist.c cfutils.c
	cc -o $@ $(CFLAGS) $(LDFLAGS) $^
