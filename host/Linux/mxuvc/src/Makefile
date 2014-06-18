LIB_NAME=mxuvc
UNAME := $(shell uname)

# Default backends
AUDIO?=libusb-uac
VIDEO?=libusb-uvc
PLUGINS?=alert burnin custom_control

SRC=src/common/common.c src/common/debug.c src/common/qbox.c \
    $(wildcard src/video/${VIDEO}/*.c) \
    $(wildcard src/audio/${AUDIO}/*.c) \
    $(foreach plugin,$(PLUGINS),$(wildcard src/plugins/$(plugin)/*.c))


CFLAGS+=-ggdb
CFLAGS+=-fno-common -fstrict-overflow
CFLAGS+=-Wall \
	-Wcast-align -Wcast-qual -Wextra \
	-Wno-empty-body -Wno-unused-parameter \
	-Wshadow -Wwrite-strings -Wswitch-default
CFLAGS+=-Iinclude -Isrc/common \
	-Isrc/video/${VIDEO} -Isrc/audio/${AUDIO} -Isrc/plugins/alert -Isrc/plugins/burnin -Isrc/plugins/custom_control

-include src/video/${VIDEO}/extra.mk
-include src/audio/${AUDIO}/extra.mk
-include $(foreach plugin,$(PLUGINS),src/plugins/$(plugin)/extra.mk)

OBJ=$(patsubst %.c,%.o,$(SRC))

all: check lib$(LIB_NAME).a shared

lib$(LIB_NAME).a: $(OBJ)
	$(AR) rc lib/lib$(LIB_NAME).a $(OBJ)

LIBUSB=1
ifeq ($(UNAME), Linux)
ifeq ($(CC), cc) #chceck if its x86 target
ifeq ($(wildcard /etc/fedora-release),) 
else
ifeq ($(wildcard /usr/include/libusb-1.0/libusb.h),)
	LIBUSB=0
else
endif
endif
endif
CFLAGS+=-Wno-array-bounds -Wno-ignored-qualifiers
LDFLAGS+=-lrt
shared:
	$(CC) $(LDFLAGS) $(OBJ) -fPIC -shared \
		-Wl,-soname,lib$(LIB_NAME).so -o lib/lib$(LIB_NAME).so
endif
ifeq ($(UNAME), Darwin)
shared:
	$(CC) $(LDFLAGS) -dynamiclib -Wl,-headerpad_max_install_names,-undefined,dynamic_lookup,-compatibility_version,1.0,-current_version,1.0,-install_name,/usr/local/lib/lib$(LIB_NAME).1.dylib -o lib$(LIB_NAME).1.dylib $(OBJ)
endif

check:
ifeq ($(LIBUSB), 0)
	@echo ERROR: libusb devel package is not installed 
	@echo run 'yum install libusb1-devel' to install libusb in x86 fc machine 
	exit 1 
endif
	
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(OBJ) lib/lib$(LIB_NAME).so lib/lib$(LIB_NAME).a lib$(LIB_NAME).1.dylib

distclean: clean
	make clean
	find . -name "*.o" -exec rm {} \;

.PHONY: lib clean distclean all check
