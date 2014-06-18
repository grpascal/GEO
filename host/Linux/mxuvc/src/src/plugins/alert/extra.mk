# Include the common libusb function if not included yet
ifndef COMMON_LIBUSB
COMMON_LIBUSB=1
SRC+=$(wildcard src/common/libusb/*.c)
endif
