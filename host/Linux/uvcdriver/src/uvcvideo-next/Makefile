KERNEL_VERSION	:= `uname -r`
KERNEL_DIR	:= /lib/modules/$(KERNEL_VERSION)/build
PWD		:= $(shell pwd)

CPPFLAGS        := -I$(M)/include $(CPPFLAGS)
KBUILD_CPPFLAGS := -I$(M)/include $(KBUILD_CPPFLAGS)
LINUXINCLUDE    := -I$(M)/include $(LINUXINCLUDE)
EXTRA_CFLAGS	:= -include $(M)/compat.h


uvcvideo-objs  := uvc_driver.o uvc_queue.o uvc_v4l2.o uvc_video.o uvc_ctrl.o \
		  uvc_status.o uvc_isight.o
obj-m += uvcvideo.o

all:
	@echo "Building USB Video Class driver..."
	@($(MAKE) -C $(KERNEL_DIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) modules)

clean:
	@echo "Cleaning USB Video Class driver..."
	@($(MAKE) -C $(KERNEL_DIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) clean)
