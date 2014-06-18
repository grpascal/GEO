APP=alert
APP_AUDIO=alert_audio
BASEDIR=../../..
LIBMXUVC=libmxuvc.a
AUDIO?=alsa
VIDEO?=v4l2
PLUGINS=alert

-include $(BASEDIR)/src/video/${VIDEO}/ld.mk
-include $(BASEDIR)/src/audio/${AUDIO}/ld.mk
-include $(foreach plugin,$(PLUGINS),$(BASEDIR)/src/plugins/$(plugin)/ld.mk)

CFLAGS+=-ggdb
CFLAGS+=-I$(BASEDIR)/include
CFLAGS+=-D'VIDEO_BACKEND="$(VIDEO)"' -D'AUDIO_BACKEND="$(AUDIO)"'

all: $(APP) $(APP_AUDIO)

$(APP): $(LIBMXUVC) $(APP).o
	$(CC) $(LDFLAGS) $(APP).o $(LIBMXUVC) -o $(APP)
$(APP_AUDIO): $(LIBMXUVC) $(APP_AUDIO).o
	$(CC) $(LDFLAGS) $(APP_AUDIO).o $(LIBMXUVC) -o $(APP_AUDIO)

.c.o:
	$(CC) $(CFLAGS) -c $<

$(LIBMXUVC):
	make clean libmxuvc.a -C $(BASEDIR) \
		VIDEO=$(VIDEO) \
		AUDIO=$(AUDIO) \
		PLUGINS=$(PLUGINS)
	mv $(BASEDIR)/lib/$(LIBMXUVC) .
clean:
	rm -f $(APP).o $(APP) $(LIBMXUVC) $(APP_AUDIO).o $(APP_AUDIO)
