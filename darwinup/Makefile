all: darwinup

CXXFLAGS=-g

Archive.o: Archive.cpp Archive.h Depot.h File.h Utils.h
Depot.o: Depot.cpp Archive.h Depot.h File.h Utils.h
Digest.o: Digest.cpp Digest.h
File.o: File.cpp Archive.h Digest.h File.h Utils.h
Utils.o: Utils.h
main.o: main.cpp Depot.h Utils.h

# libredo.o is generated from cctools_ofiles

darwinup: Archive.o Depot.o Digest.o File.o SerialSet.o Utils.o main.o \
	  libredo.o
	g++ -lcrypto -lsqlite3 -o $@ $^

clean:
	rm -f darwinup Archive.o Depot.o Digest.o File.o SerialSet.o Utils.o main.o
