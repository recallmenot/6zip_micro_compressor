CC=gcc
CFLAGS=-Wall -O2

default: compressor

compressor: compressor.o packbits.o uzlib.a heatshrink.a
	$(CC) $(CFLAGS) -I uzlib/src -I heatshrink -o compressor compressor.o packbits.o heatshrink/libheatshrink_dynamic.a uzlib/lib/libtinf.a
compressor.o: compressor.c
	$(CC) $(CFLAGS) -g -c compressor.c
#	$(CC) $(CFLAGS) -g -c compressor.c

packbits.o: packbits/packbits.c packbits/packbits.h
	$(CC) $(CFLAGS) -c packbits/packbits.c

uzlib.a:
	$(MAKE) -C uzlib

heatshrink.a:
	$(MAKE) -C heatshrink

IO_files=*.uz* *.packed *.heatshrunk *_unpacked.pbm *_deuz*.pbm *_unheatshrunk.pbm

all:
	compressor

clean:
	rm -f compressor *.o $(IO_files)
	$(MAKE) -C uzlib clean
	$(MAKE) -C heatshrink clean

