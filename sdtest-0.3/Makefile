# Makefile for sdtest

CC	= gcc
# flags for 32-bit system
#CFLAGS	= -Wall -I/usr/local/include -fstack-protector -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -g# -DDEBUG
# flags for 64-bit system
CFLAGS	= -Wall -I/usr/local/include -fstack-protector# -g# -DDEBUG
LFLAGS	= /usr/local/lib/libsgutils2.so -lpthread

INSTALL	= /usr/local
all:	sdtest process

OBJS  = sdtest.o utils.o loads.o
OBJS += io_sd.o
OBJS += io_sg.o
OBJS += al_rws.o
OBJS += al_one.o
OBJS += al_par.o

%.o:	%.c
	$(CC) $(CFLAGS) -c $<
sdtest: $(OBJS)
	$(CC) $(LFLAGS) -o $@ $^
process: process.o utils.o
	$(CC) $(LFLAGS) -o $@ $^
clean: 
	rm -f a.out *.o *~ sdtest process .process*
install:
	install -m755 sdtest $(INSTALL)/bin/
	install -m755 process $(INSTALL)/bin/
	#install -m755 sdtest.sh $(INSTALL)/bin/
	install -m644 sdtest.8 $(INSTALL)/share/man/man8/
	install -m644 process.8 $(INSTALL)/share/man/man8/
