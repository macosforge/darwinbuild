all: darwinup

CXXFLAGS=-g

Archive.o: Archive.cpp Archive.h Depot.h File.h Utils.h
Depot.o: Depot.cpp Archive.h Depot.h File.h Utils.h
Digest.o: Digest.cpp Digest.h
File.o: File.cpp Archive.h Digest.h File.h Utils.h
Utils.o: Utils.h
main.o: main.cpp Depot.h Utils.h

# libredo.o is generated from cctools_ofiles

REDO=$(shell if [ -f /usr/local/lib/libredo_prebinding.a ]; then \
		echo /usr/local/lib/libredo_prebinding.a ; else \
		echo libredo.o ; fi)

darwinup: Archive.o Depot.o Digest.o File.o SerialSet.o Utils.o main.o \
	$(REDO)
	g++ -lcrypto -lsqlite3 -o $@ $^

clean:
	rm -f darwinup Archive.o Depot.o Digest.o File.o SerialSet.o Utils.o main.o
