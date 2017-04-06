CROSS_COMPILE ?=
CC=$(CROSS_COMPILE)gcc
CFLAGS= -c -Wall -fPIC
LIB=$(CROSS_COMPILE)ar
LIBFLAGS=cru
STRIP=$(CROSS_COMPILE)strip


INCLUDES = -I .\
		   -I ./include

SG3_LIBRARY=-L./lib -lsg3-utils

SOURCES = diskio.c


_OBJECTS=$(SOURCES:.c=.o)
OBJECTS=$(patsubst %,%,$(_OBJECTS))

OUTPUT_EXECUTABLE=diskio

all: $(OUTPUT_EXECUTABLE)

$(OUTPUT_EXECUTABLE): $(OBJECTS) sg3-utils
	$(CC) $(OBJECTS) $(SG3_LIBRARY) -o $(OUTPUT_EXECUTABLE)
	
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

sg3-utils:
	@cd lib; make

clean:
	@cd lib; make clean
	rm -rf *.o $(OUTPUT_EXECUTABLE)
