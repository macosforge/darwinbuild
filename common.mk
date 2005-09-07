###
### Common makefile variables potentially set by autoconf
###
PREFIX?=/usr/local
DESTDIR?=

###
###
BINDIR=$(DESTDIR)$(PREFIX)/bin
DATDIR=$(DESTDIR)$(PREFIX)/share
INCDIR=$(DESTDIR)$(PREFIX)/include
INSTALL=install
INSTALL_EXE_FLAGS=-m 0755 -o root -g wheel
INSTALL_DIR_FLAGS=$(INSTALL_EXE_FLAGS)
INSTALL_DOC_FLAGS=-m 0644 -o root -g wheel
