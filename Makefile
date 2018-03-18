#
# Students' Makefile for the Malloc Lab
#
TEAM = bovik
VERSION = 1

CC = gcc
CFLAGS = -Wall -m32 -O2
DFLAGS = -Wall -m32 -g
TFLAGS = -Wall -m32 -g -DTRACE

OBJS = mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o
SRCS = *.c

mdriver: $(OBJS)
	$(CC) $(CFLAGS) -o mdriver $(OBJS)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h
memlib.o: memlib.c memlib.h
mm.o: mm.c mm.h memlib.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h

debug: $(SRCS)
	$(CC) $(DFLAGS) -o mdriver_debug $(SRCS)

trace: $(SRCS)
	$(CC) $(TFLAGS) -o mdriver_trace $(SRCS)

clean:
	rm -f *~ *.o mdriver mdriver_debug mdriver_trace
