LIBUSB_TARBALL=$(wildcard ../tarballs/libusb-*.tar.bz2)

all: linux/lib/libusb-1.0.a

linux/lib/libusb-1.0.a: linux/configure
	@cd linux/; \
	CC="$(CC)" AR="$(AR)" ./configure --prefix=$$PWD $(LIBUSB_CONFIG_OPT) ;\
	make all install

linux/configure: $(LIBUSB_TARBALL)
	tar xmjf $(LIBUSB_TARBALL)
	rm -fr linux
	mv $(shell basename $(LIBUSB_TARBALL:.tar.bz2=)) linux;

clean:
	rm -fr linux

.PHONY: all clean
