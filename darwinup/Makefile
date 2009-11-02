include ../common.mk

CXXFLAGS=-g
# libredo.o is generated from cctools_ofiles
REDO=$(shell if [ -f /usr/local/lib/libredo_prebinding.a ]; then \
		echo /usr/local/lib/libredo_prebinding.a ; else \
		echo libredo.o ; fi)


install: all
	[ -d $(BINDIR) ] || $(INSTALL) -d $(INSTALL_DIR_FLAGS) $(BINDIR)
	$(INSTALL) $(INSTALL_EXE_FLAGS) darwinup $(BINDIR)


all: darwinup

darwinup: Archive.o Depot.o Digest.o File.o SerialSet.o Utils.o main.o \
	$(REDO)
	g++ -lcrypto -lsqlite3 -o $@ $^

Archive.o: Archive.cpp Archive.h Depot.h File.h Utils.h
Depot.o: Depot.cpp Archive.h Depot.h File.h Utils.h
Digest.o: Digest.cpp Digest.h
File.o: File.cpp Archive.h Digest.h File.h Utils.h
Utils.o: Utils.h
main.o: main.cpp Depot.h Utils.h

clean:
	rm -f darwinup Archive.o Depot.o Digest.o File.o SerialSet.o Utils.o main.o
