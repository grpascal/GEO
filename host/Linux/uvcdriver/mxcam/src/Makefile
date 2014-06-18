EXECUTABLE=mxcam

SRC=mxcam.c
OBJ=$(SRC:.c=.o)

UNAME := $(shell uname)

RELEASE_NAME=$(shell \
    if [ -f ../RELEASE_NAME ]; then \
	    cat ../RELEASE_NAME; \
    fi)

# Try to get the version from svnversion
VERSION:=$(shell (which svnversion > /dev/null && svnversion) \
	|| (echo "exported"))
# If the code is "exported", then use VERSION file if available.
VERSION:=$(shell \
	if [ "X$(VERSION)" = "Xexported" -a -e VERSION ]; then \
		cat VERSION; \
	else \
		echo $(RELEASE_NAME) Build $(VERSION); \
	fi)


CFLAGS+=-D'MXCAM_VERSION="$(VERSION)"'
CFLAGS+=-ggdb
CFLAGS+=-Wall \
	-Wcast-align -Wcast-qual -Wextra \
	-Wno-empty-body -Wno-unused-parameter \
	-Wshadow -Wwrite-strings -Wswitch-default
CFLAGS+=-Ilibmxcam
LDFLAGS+=-L./libusb/linux/lib/ -L./libmxcam/ -lmxcam -lusb-1.0

ifeq ($(UNAME), Linux)
CFLAGS+=-Wno-array-bounds -Wno-ignored-qualifiers
LDFLAGS+=-lpthread -lrt -static
endif

all: depend $(EXECUTABLE)

depend:
	@make all -C libusb
	@make all -C libmxcam

$(EXECUTABLE): libusb/linux/lib/libusb-1.0.a libmxcam/libmxcam.a $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(EXECUTABLE)
	@make clean -C libmxcam

distclean: clean
	@make clean -C libusb

.PHONY: clean distclean all
