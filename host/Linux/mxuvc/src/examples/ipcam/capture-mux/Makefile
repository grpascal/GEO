MUXAPP=capture_mux
AUDIOAPP=capture_audmux
AVAPP=capture_avmux
RAWAPP=capture_raw

BASEDIR=../../..
AUDIO?=alsa
VIDEO?=v4l2
LIBMXUVC=libmxuvc.a
PLUGINS=alert

-include $(BASEDIR)/src/video/${VIDEO}/ld.mk
-include $(BASEDIR)/src/audio/${AUDIO}/ld.mk
-include $(foreach plugin,$(PLUGINS),$(BASEDIR)/src/plugins/$(plugin)/ld.mk)
CFLAGS+=-ggdb
CFLAGS+=-I$(BASEDIR)/include -I$(BASEDIR)/src/common
CFLAGS+=-D'VIDEO_BACKEND="$(VIDEO)"' -D'AUDIO_BACKEND="$(AUDIO)"'

all: $(MUXAPP) $(AUDIOAPP) $(AVAPP) $(RAWAPP)

$(MUXAPP): $(LIBMXUVC) $(MUXAPP).o
	$(CC) $(MUXAPP).o $(LIBMXUVC) $(LDFLAGS) -o $(MUXAPP)
$(AUDIOAPP):  $(LIBMXUVC) $(AUDIOAPP).o
	$(CC) $(AUDIOAPP).o $(LIBMXUVC) $(LDFLAGS) -o $(AUDIOAPP)
$(AVAPP): $(LIBMXUVC) $(AVAPP).o
	$(CC) $(AVAPP).o $(LIBMXUVC) $(LDFLAGS) -o $(AVAPP)
$(RAWAPP): $(LIBMXUVC) $(RAWAPP).o
	$(CC) $(RAWAPP).o $(LIBMXUVC) $(LDFLAGS) -o $(RAWAPP)

.c.o:
	$(CC) $(CFLAGS) -c $<

$(LIBMXUVC):
	make clean libmxuvc.a -C $(BASEDIR) \
		VIDEO=$(VIDEO) \
		AUDIO=$(AUDIO)
	mv $(BASEDIR)/lib/$(LIBMXUVC) .
clean:
	rm -f $(APP).o $(MUXAPP).o $(AUDIOAPP).o $(MUXAPP) $(AUDIOAPP) $(AVAPP).o $(AVAPP) $(LIBMXUVC) $(RAWAPP).o 
