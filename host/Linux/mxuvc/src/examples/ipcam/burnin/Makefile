APP=burnin_test
BASEDIR=../../..
LIBMXUVC=libmxuvc
#LIB_NAME=mxuvc
AUDIO?=alsa
VIDEO?=v4l2
PLUGINS?=burnin alert

SRC=$(BASEDIR)/src/common/common.c $(BASEDIR)/src/common/debug.c $(BASEDIR)/src/common/qbox.c \
    $(wildcard $(BASEDIR)/src/video/${VIDEO}/*.c) \
    $(wildcard $(BASEDIR)/src/audio/${AUDIO}/*.c) \
    $(foreach plugin,$(PLUGINS),$(wildcard $(BASEDIR)/src/plugins/$(plugin)/*.c))

-include $(BASEDIR)/src/video/${VIDEO}/ld.mk
-include $(BASEDIR)/src/audio/${AUDIO}/ld.mk
-include $(foreach plugin,$(PLUGINS),$(BASEDIR)/src/plugins/$(plugin)/ld.mk)

CFLAGS+=-ggdb
CFLAGS+=-I$(BASEDIR)/include -I$(BASEDIR)/src/common \
	-I$(BASEDIR)/src/video/${VIDEO} -I$(BASEDIR)/src/audio/${AUDIO} \
	-I$(BASEDIR)/src/plugins/alert -I$(BASEDIR)/src/plugins/burnin \
	-I$(BASEDIR)/src/common/libskypeecxu

CFLAGS+=-D'VIDEO_BACKEND="$(VIDEO)"' -D'AUDIO_BACKEND="$(AUDIO)"'

OBJ=$(patsubst %.c,%.o,$(SRC))

all: $(APP) $(APP_AUDIO)

$(APP): $(LIBMXUVC).a $(APP).o
	$(CC) $(LDFLAGS) $(APP).o $(LIBMXUVC).a -o $(APP)
$(APP_AUDIO): $(LIBMXUVC).a $(APP_AUDIO).o
	$(CC) $(LDFLAGS) $(APP_AUDIO).o $(LIBMXUVC).a -o $(APP_AUDIO)
.c.o:
	$(CC) $(CFLAGS) -c $<

$(LIBMXUVC).a: $(OBJ)
	make clean $(LIBMXUVC).a -C $(BASEDIR) AUDIO=$(AUDIO) VIDEO=$(VIDEO)
	mv $(BASEDIR)/lib/$(LIBMXUVC).a .
clean:
	rm -f $(APP).o $(APP) $(LIBMXUVC).a
