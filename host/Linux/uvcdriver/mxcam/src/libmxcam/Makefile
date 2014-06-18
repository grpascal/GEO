UNAME=$(shell uname)
CFLAGS+=-ggdb
CFLAGS+=-Wall \
	-Wcast-align -Wcast-qual -Wextra \
	-Wno-empty-body -Wno-unused-parameter \
	-Wshadow -Wwrite-strings -Wswitch-default
CFLAGS+=-I../libusb/linux/include
LDFLAGS_LINUX=-L../libusb/linux/lib -lpthread -lrt

ifeq ($(UNAME), Linux)
all: libmxcam.o libmxcam.a libmxcam.so.1.0
CFLAGS+=-Wno-array-bounds -Wno-ignored-qualifiers
clean-shared:
	rm -rf libmxcam.so.1.0
endif
ifeq ($(UNAME), Darwin)
all: libmxcam.o libmxcam.a libmxcam.1.dylib
clean-shared:
	rm -rf libmxcam.1.dylib
endif

libmxcam.o: libmxcam.c libmxcam.h
	$(CC) $(CFLAGS) -o libmxcam.o -c libmxcam.c
libmxcam.a: libmxcam.o
	$(AR) rc libmxcam.a libmxcam.o

libmxcam.so.1.0: libmxcam.c libmxcam.h
	$(CC) $(CFLAGS) $(LDFLAGS_LINUX) -lusb-1.0 -fPIC -shared -Wl,-soname,libmxcam.so.1 -o libmxcam.so.1.0
libmxcam.1.dylib: libmxcam.o
	$(CC) -dynamiclib -Wl,-headerpad_max_install_names,-undefined,dynamic_lookup,-compatibility_version,1.0,-current_version,1.0,-install_name,/usr/local/lib/libmxcam.1.dylib -o libmxcam.1.dylib libmxcam.o

clean: clean-shared
	rm -rf libmxcam.o libmxcam.a

.PHONY: all shared clean clean-shared
