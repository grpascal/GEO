
#AUDIO?=libusb-uac
#VIDEO?=libusb-uvc
#AUDIO?=alsa
VIDEO?=v4l2

LOCAL_PATH:= $(call my-dir)

common_CFLAGS := -Wall -fexceptions

include $(CLEAR_VARS)

LOCAL_SRC_FILES := simple-capture.c

LOCAL_CFLAGS += $(common_CFLAGS)

LOCAL_MODULE := simple-capture

LOCAL_MODULE_TAGS := optional

#=======================================================
LOCAL_C_INCLUDES += ./../mxuvc

LOCAL_SHARED_LIBRARIES := libmxuvc 

LOCAL_LDLIBS := -lpthread -ldl

LOCAL_CFLAGS += -ggdb -DVIDEO_ONLY -D'VIDEO_BACKEND="$(VIDEO)"' -D'AUDIO_BACKEND="$(AUDIO)"'		

#=======================================================

include $(BUILD_EXECUTABLE)


