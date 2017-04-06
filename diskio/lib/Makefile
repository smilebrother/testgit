CROSS_COMPILE ?=
CC=$(CROSS_COMPILE)gcc
CFLAGS= -c -Wall -fPIC
LIB=$(CROSS_COMPILE)ar
LIBFLAGS=cru
STRIP=$(CROSS_COMPILE)strip


INCLUDES = -I .\
		   -I ./../include

CFLAGS += -DSG_LIB_LINUX -DSG_IO

SOURCES = \
		 sg_lib.c \
		 sg_lib.c \
		 sg_lib_data.c \
		 sg_cmds_basic.c \
		 sg_cmds_basic2.c \
		 sg_cmds_extra.c \
		 sg_cmds_mmc.c \
		 sg_pt_common.c \
		 sg_pt_linux.c \
		 sg_io_linux.c


_OBJECTS=$(SOURCES:.c=.o)
OBJECTS=$(patsubst %,%,$(_OBJECTS))

BUILD_LIBRARY=libsg3-utils
BUILD_LIBRARIES=$(BUILD_LIBRARY).a

all: $(BUILD_LIBRARIES)

$(BUILD_LIBRARY).a: $(OBJECTS)
	$(LIB) $(LIBFLAGS) $@ $(OBJECTS)
	
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

clean:
	rm -rf *.o *.a
