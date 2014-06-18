# get the release name
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

# External libraries
LIBAVCODEC=$(shell env PKG_CONFIG_PATH=./ffmpeg/lib/pkgconfig pkg-config \
	   --define-variable=prefix=ffmpeg --libs libswscale libavcodec)
LIBLUA=$(shell env PKG_CONFIG_PATH=./lua/etc pkg-config \
       --define-variable=prefix=lua --libs lua)

FFMPEG_TARBALL=$(wildcard tarballs/ffmpeg-*.tar.bz2)
FFMPEG_PATCHES=$(wildcard patch/ffmpeg/*)
LUA_TARBALL=$(wildcard tarballs/lua-*.tar.gz)
LUA_PATCHES=$(wildcard patch/lua/*)

# Common flags
CFLAGS=-Wall -O3 -D'MXVIEW_VERSION="$(VERSION)"'
SRC=mxview.c uvcext.c

ifdef NOAUDIO
CFLAGS+=-DNOAUDIO
else
SRC+=audio.c
LIB+=-lasound
endif

ifdef NOLUA
CFLAGS+=-DNOLUA
else
CFLAGS+=-Ilua/include
SRC+=luatest.c
LIB+=$(LIBLUA) -lpthread -ldl
endif

ifdef NOX
CFLAGS+=-DNOX
else
CFLAGS+=-Iffmpeg/include -I. $(shell pkg-config gtk+-2.0 gthread-2.0 --cflags)
SRC+=colorspace.c \
	decode.c \
	gui.c \
	jpegutils.c \
	mjpeg_logging.c \
	video_window.c
LIB+=$(LIBAVCODEC) $(shell pkg-config gtk+-2.0 gthread-2.0 --libs) \
	-lX11 -lXv -ljpeg -lXinerama -lrt
endif

OBJ=$(SRC:.c=.o)

# Main targets
mxview: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIB) -o $@

# For backward compatibility, 
.PHONY: mxview-nox
mxview-nox:
	@echo mxview-nox can be built with the following commands:
	@echo "	make clean"
	@echo "	make mxview NOX=1"
	@echo "	mv mxview mxview-nox"
	@false

# Build the third party depedencies before building those files
luatest.o: lua/built
decode.o: ffmpeg/built
colorspace.o: ffmpeg/built
video_window.o: ffmpeg/built

# Targets to retreive third party libraries source code and apply patches
ffmpeg/$(FFMPEG_VER): $(FFMPEG_PATCHES)
	tar xjf $(FFMPEG_TARBALL);
	@rm -fr ffmpeg; \
	mv $(shell basename $(FFMPEG_TARBALL:.tar.bz2=)) ffmpeg; \
	for patch in $(FFMPEG_PATCHES); do \
		patch -d ffmpeg -p0 < $${patch}; \
	done; \
	touch ffmpeg/$(FFMPEG_VER)

lua/$(LUA_VER): $(LUA_PATCHES)
	tar xzf $(LUA_TARBALL);
	@rm -fr lua; \
	mv $(shell basename $(LUA_TARBALL:.tar.gz=)) lua; \
	for patch in $(LUA_PATCHES); do \
		patch -d ffmpeg -p0 < $${patch}; \
	done; \
	touch lua/$(LUA_VER)

# Targets to build third party libraries
ffmpeg/built: ffmpeg/$(FFMPEG_VER)
	@cd ffmpeg;\
	./configure --prefix=. --disable-ffmpeg --disable-ffplay \
		--disable-ffprobe --disable-ffserver --disable-avdevice \
		--disable-avformat --disable-swscale-alpha --disable-everything \
		--enable-decoder=h264; \
	make all ; \
	make install; \
	touch built

lua/built: lua/$(LUA_VER)
	@cd lua; \
	make linux; \
	make local; \
	touch built

# Cleaning
clean:
	rm -f mxview *.o

distclean: clean
	rm -fr ffmpeg
	rm -rf lua

.PHONY: clean distclean
