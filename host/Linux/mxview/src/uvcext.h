/*******************************************************************************
*
* The content of this file or document is CONFIDENTIAL and PROPRIETARY
* to Maxim Integrated Products.  It is subject to the terms of a
* License Agreement between Licensee and Maxim Integrated Products.
* restricting among other things, the use, reproduction, distribution
* and transfer.  Each of the embodiments, including this information and
* any derivative work shall retain this copyright notice.
*
* Copyright (c) 2011 Maxim Integrated Products.
* All rights reserved.
*
*******************************************************************************/

#ifndef _UVCEXT_H
#define _UVCEXT_H

#include "videodev2.h"

#define V4L2_CID_XU_BASE		V4L2_CID_PRIVATE_BASE

#define V4L2_CID_XU_AVC_PROFILE		(V4L2_CID_XU_BASE + 0)
#define V4L2_CID_XU_AVC_LEVEL		(V4L2_CID_XU_BASE + 1)
#define V4L2_CID_XU_PICTURE_CODING	(V4L2_CID_XU_BASE + 2)
#define V4L2_CID_XU_RESOLUTION		(V4L2_CID_XU_BASE + 3)
#define V4L2_CID_XU_RESOLUTION2		(V4L2_CID_XU_BASE + 4)
#define V4L2_CID_XU_GOP_STRUCTURE	(V4L2_CID_XU_BASE + 5)
#define V4L2_CID_XU_GOP_LENGTH		(V4L2_CID_XU_BASE + 6)
#define V4L2_CID_XU_FRAME_RATE		(V4L2_CID_XU_BASE + 7)
#define V4L2_CID_XU_BITRATE		(V4L2_CID_XU_BASE + 8)
#define V4L2_CID_XU_FORCE_I_FRAME	(V4L2_CID_XU_BASE + 9)
#define V4L2_CID_XU_GET_VERSION		(V4L2_CID_XU_BASE + 10)
#define V4L2_CID_XU_MAX_NAL		(V4L2_CID_XU_BASE + 11)

#define V4L2_CID_SKYPE_XU_VERSION		(V4L2_CID_XU_BASE + 12)
#define V4L2_CID_SKYPE_XU_LASTERROR		(V4L2_CID_XU_BASE + 13)
#define V4L2_CID_SKYPE_XU_FIRMWAREDAYS		(V4L2_CID_XU_BASE + 14)
#define V4L2_CID_SKYPE_XU_STREAMID		(V4L2_CID_XU_BASE + 15)
#define V4L2_CID_SKYPE_XU_ENDPOINT_SETTING	(V4L2_CID_XU_BASE + 16)

#define V4L2_CID_SKYPE_XU_STREAMFORMATPROBE			(V4L2_CID_XU_BASE + 17)
#define V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT			(V4L2_CID_XU_BASE + 22)
#define V4L2_CID_SKYPE_XU_STREAMFORMATPROBE_TYPE 		(V4L2_CID_XU_BASE + 23)
#define V4L2_CID_SKYPE_XU_STREAMFORMATPROBE_WIDTH 		(V4L2_CID_XU_BASE + 24)
#define V4L2_CID_SKYPE_XU_STREAMFORMATPROBE_HEIGHT 		(V4L2_CID_XU_BASE + 25)
#define V4L2_CID_SKYPE_XU_STREAMFORMATPROBE_FRAMEINTERVAL 	(V4L2_CID_XU_BASE + 26)


#define V4L2_CID_SKYPE_XU_BITRATE		(V4L2_CID_XU_BASE + 27)
#define V4L2_CID_SKYPE_XU_FRAMEINTERVAL		(V4L2_CID_XU_BASE + 28)
#define V4L2_CID_SKYPE_XU_GENERATEKEYFRAME	(V4L2_CID_XU_BASE + 29)

#define V4L2_CID_XU_VUI_ENABLE		(V4L2_CID_XU_BASE + 30)
#define V4L2_CID_XU_PIC_TIMING_ENABLE 	(V4L2_CID_XU_BASE + 31)
#define V4L2_CID_XU_AV_MUX_ENABLE		(V4L2_CID_XU_BASE + 32)
#define V4L2_CID_XU_MAX_FRAME_SIZE	    (V4L2_CID_XU_BASE + 33)

#define V4L2_CID_PU_XU_ANF_ENABLE	(V4L2_CID_XU_BASE + 42)
#define V4L2_CID_PU_XU_NF_STRENGTH	(V4L2_CID_XU_BASE + 43)
#define V4L2_CID_PU_XU_TF_STRENGTH	(V4L2_CID_XU_BASE + 44)
#define V4L2_CID_PU_XU_ADAPTIVE_WDR_ENABLE	(V4L2_CID_XU_BASE + 45)
#define V4L2_CID_PU_XU_WDR_STRENGTH	(V4L2_CID_XU_BASE + 46)
#define V4L2_CID_PU_XU_AUTO_EXPOSURE	(V4L2_CID_XU_BASE + 47)
#define V4L2_CID_PU_XU_EXPOSURE_TIME	(V4L2_CID_XU_BASE + 48)
#define V4L2_CID_PU_XU_AUTO_WHITE_BAL	(V4L2_CID_XU_BASE + 49)
#define V4L2_CID_PU_XU_WHITE_BAL_TEMP	(V4L2_CID_XU_BASE + 50)
#define V4L2_CID_PU_XU_VFLIP            (V4L2_CID_XU_BASE + 51)
#define V4L2_CID_PU_XU_HFLIP            (V4L2_CID_XU_BASE + 52)
#define V4L2_CID_PU_XU_WB_ZONE_SEL_ENABLE (V4L2_CID_XU_BASE + 53)
#define V4L2_CID_PU_XU_WB_ZONE_SEL      (V4L2_CID_XU_BASE + 54)
#define V4L2_CID_PU_XU_EXP_ZONE_SEL_ENABLE (V4L2_CID_XU_BASE + 55)
#define V4L2_CID_PU_XU_EXP_ZONE_SEL     (V4L2_CID_XU_BASE + 56)
#define V4L2_CID_PU_XU_MAX_ANALOG_GAIN  (V4L2_CID_XU_BASE + 57)
#define V4L2_CID_PU_XU_HISTO_EQ         (V4L2_CID_XU_BASE + 58)
#define V4L2_CID_PU_XU_SHARPEN_FILTER   (V4L2_CID_XU_BASE + 59)
#define V4L2_CID_PU_XU_GAIN_MULTIPLIER  (V4L2_CID_XU_BASE + 60)
#define V4L2_CID_PU_XU_CROP             (V4L2_CID_XU_BASE + 61)
#define V4L2_CID_PU_XU_CROP_ENABLE      (V4L2_CID_XU_BASE + 61)
#define V4L2_CID_PU_XU_CROP_WIDTH       (V4L2_CID_XU_BASE + 62)
#define V4L2_CID_PU_XU_CROP_HEIGHT      (V4L2_CID_XU_BASE + 63)
#define V4L2_CID_PU_XU_CROP_X           (V4L2_CID_XU_BASE + 64)
#define V4L2_CID_PU_XU_CROP_Y           (V4L2_CID_XU_BASE + 65)
#define V4L2_CID_PU_XU_EXP_MIN_FR_RATE  (V4L2_CID_XU_BASE + 66)
#define V4L2_CID_PU_XU_MVMT_QUERY	(V4L2_CID_XU_BASE + 67)

#define V4L2_CID_XU_END			(V4L2_CID_XU_BASE + 68)

void init_ctrl(int fd);
void list_ctrl(int fd);
int get_v4l2_ctrl(int id, int *value);
int set_v4l2_ctrl(int id, int value);
int get_v4l2_framerate();
int set_v4l2_framerate(int fps);

struct StreamFormat {
	__u8 bStreamType;	/* As per StreamFormat listing. */
	__u16 wWidth;		/* Frame width in pixels. */
	__u16 wHeight;		/* Frame height in pixels. */
	__u32 dwFrameInterval;	/* Frame interval in 100 ns units */
	__u32 dwBitrate;	/* Bitrate in bits per second */
} __attribute__ ((packed));

int get_skype_stream_control(int fd, struct StreamFormat *format, int commit);
int set_skype_stream_control(int fd, struct StreamFormat *format, int commit);

#define NUM_XU_RES 17
struct mapping {
	char	name[9];
	int	id;
	int	id2;
};

struct mapping xu_res_mapping[NUM_XU_RES];

/* Crop mode */
struct crop_info {
	int enable;
	int width;
	int height;
	int x;
	int y;
};


int get_v4l2_crop(struct crop_info *info);
int set_v4l2_crop(struct crop_info *info);

#endif /* _UVCEXT_H */
