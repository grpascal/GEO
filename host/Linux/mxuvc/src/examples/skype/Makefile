APP=simple-capture
APP_AUDIO=simple-capture-audioOnly
BASEDIR=../..
AUDIO?=alsa
VIDEO?=v4l2
PLUGINS=alert
#AUDIO?=libusb-uac
#VIDEO?=libusb-uvc
LIBMXUVC=libmxuvc.a

-include $(BASEDIR)/src/video/${VIDEO}/ld.mk
-include $(BASEDIR)/src/audio/${AUDIO}/ld.mk
-include $(foreach plugin,$(PLUGINS),$(BASEDIR)/src/plugins/$(plugin)/ld.mk)

CFLAGS+=-ggdb
CFLAGS+=-I$(BASEDIR)/include
CFLAGS+=-D'VIDEO_BACKEND="$(VIDEO)"' -D'AUDIO_BACKEND="$(AUDIO)"'

all: $(APP) $(APP_AUDIO)

$(APP): $(LIBMXUVC) $(APP).o
	$(CC) $(APP).o $(LIBMXUVC) $(LDFLAGS) -o $(APP)
$(APP_AUDIO): $(LIBMXUVC) $(APP_AUDIO).o
	$(CC) $(APP_AUDIO).o $(LIBMXUVC) $(LDFLAGS) -o $(APP_AUDIO)

.c.o:
	$(CC) $(CFLAGS) -c $<

$(LIBMXUVC):
	make clean libmxuvc.a -C $(BASEDIR) \
		VIDEO=$(VIDEO) \
		AUDIO=$(AUDIO)
	mv $(BASEDIR)/lib/$(LIBMXUVC) .
clean:
	rm -f $(APP).o $(APP_AUDIO).o $(APP) $(LIBMXUVC) $(APP_AUDIO)
