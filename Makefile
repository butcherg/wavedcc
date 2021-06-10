#out-of-tree build is supported by copying the Makefile to 
#the build directory and editing the srcdir= to contain
#the path to the wavedcc director.
srcdir=

#to cross-compile, add the target architecture to the CC 
#value, e.g. CC=aarch64-linux-g++. Your path needs to 
#include the location of the build tools.
CC=g++

#umcomment for libpigpio:
#CFLAGS=-Wall -pthread
#LDFLAGS=-pthread -lpigpio -lrt

#or, uncomment for libpigpiod:
CFLAGS=-Wall -pthread -DUSE_PIGPIOD_IF
LDFLAGS=-pthread -lpigpiod_if2 -lrt

all:  wavedccd wavedcc

wavedccd: wavedccd.o dccengine.o dccpacket.o 
	$(CC) -o wavedccd wavedccd.o dccpacket.o dccengine.o $(LDFLAGS)
	
wavedccd.o: $(srcdir)wavedccd.cpp $(srcdir)dccengine.h
	$(CC) $(CFLAGS) -o wavedccd.o -c $(srcdir)wavedccd.cpp


wavedcc: wavedcc.o dccengine.o dccpacket.o 
	$(CC) -o wavedcc wavedcc.o dccpacket.o dccengine.o $(LDFLAGS)
	
wavedcc.o: $(srcdir)wavedcc.cpp $(srcdir)dccengine.h
	$(CC) $(CFLAGS) -o wavedcc.o -c $(srcdir)wavedcc.cpp
	

dccengine.o: $(srcdir)dccengine.cpp
	$(CC) $(CFLAGS) -o dccengine.o -c $(srcdir)dccengine.cpp

dccpacket.o: $(srcdir)dccpacket.cpp
	$(CC) $(CFLAGS) -o dccpacket.o -c $(srcdir)dccpacket.cpp

clean:
	rm -rf *.o wavedccd wavedcc

