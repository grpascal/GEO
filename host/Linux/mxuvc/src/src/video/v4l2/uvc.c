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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h> /* open() */
#include <unistd.h> /* close() */
#include <errno.h>
#include <string.h> /* memset, memcmp */
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h> /* mmap() */
#include <pthread.h>
//#include <unistd.h> /* sleep */
#include <assert.h> /* assert */

#include "common.h"
#include "qbox.h"
#include "mxuvc.h"
#include "uvcvideo.h"
#include "videodev2.h"
#include "skypeecxuparser.h"

#define BUFFER_COUNT 8
#define V4L2_CID_XU_BASE                   V4L2_CID_PRIVATE_BASE

#define V4L2_CID_XU_AVC_PROFILE            (V4L2_CID_XU_BASE + 0)
#define V4L2_CID_XU_AVC_LEVEL              (V4L2_CID_XU_BASE + 1)
#define V4L2_CID_XU_PICTURE_CODING         (V4L2_CID_XU_BASE + 2)
#define V4L2_CID_XU_RESOLUTION             (V4L2_CID_XU_BASE + 3)
#define V4L2_CID_XU_RESOLUTION2            (V4L2_CID_XU_BASE + 4)
#define V4L2_CID_XU_GOP_STRUCTURE          (V4L2_CID_XU_BASE + 5)
#define V4L2_CID_XU_GOP_LENGTH             (V4L2_CID_XU_BASE + 6)
#define V4L2_CID_XU_FRAME_RATE             (V4L2_CID_XU_BASE + 7)
#define V4L2_CID_XU_BITRATE                (V4L2_CID_XU_BASE + 8)
#define V4L2_CID_XU_FORCE_I_FRAME          (V4L2_CID_XU_BASE + 9)
#define V4L2_CID_XU_GET_VERSION            (V4L2_CID_XU_BASE + 10)
#define V4L2_CID_XU_MAX_NAL                (V4L2_CID_XU_BASE + 11)

#define V4L2_CID_XU_VUI_ENABLE             (V4L2_CID_XU_BASE + 30)
#define V4L2_CID_XU_PIC_TIMING_ENABLE      (V4L2_CID_XU_BASE + 31)
#define V4L2_CID_XU_AV_MUX_ENABLE          (V4L2_CID_XU_BASE + 32)
#define V4L2_CID_XU_MAX_FRAME_SIZE         (V4L2_CID_XU_BASE + 33)

/* Skype SECS XU */
#define V4L2_CID_SKYPE_XU_VERSION                         (V4L2_CID_XU_BASE + 12)
#define V4L2_CID_SKYPE_XU_LASTERROR                       (V4L2_CID_XU_BASE + 13)
#define V4L2_CID_SKYPE_XU_FIRMWAREDAYS                    (V4L2_CID_XU_BASE + 14)
#define V4L2_CID_SKYPE_XU_STREAMID                        (V4L2_CID_XU_BASE + 15)
#define V4L2_CID_SKYPE_XU_ENDPOINT_SETTING                (V4L2_CID_XU_BASE + 16)

#define V4L2_CID_SKYPE_XU_STREAMFORMATPROBE               (V4L2_CID_XU_BASE + 17)
#define V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT              (V4L2_CID_XU_BASE + 22)
#define V4L2_CID_SKYPE_XU_STREAMFORMATPROBE_TYPE          (V4L2_CID_XU_BASE + 23)
#define V4L2_CID_SKYPE_XU_STREAMFORMATPROBE_WIDTH         (V4L2_CID_XU_BASE + 24)
#define V4L2_CID_SKYPE_XU_STREAMFORMATPROBE_HEIGHT        (V4L2_CID_XU_BASE + 25)
#define V4L2_CID_SKYPE_XU_STREAMFORMATPROBE_FRAMEINTERVAL (V4L2_CID_XU_BASE + 26)

#define V4L2_CID_SKYPE_XU_BITRATE                         (V4L2_CID_XU_BASE + 27)
#define V4L2_CID_SKYPE_XU_FRAMEINTERVAL                   (V4L2_CID_XU_BASE + 28)
#define V4L2_CID_SKYPE_XU_GENERATEKEYFRAME                (V4L2_CID_XU_BASE + 29)

//mux channel xu's
//mux1 h264
#define V4L2_CID_MUX_CH1_XU_RESOLUTION		(V4L2_CID_XU_BASE + 0)
#define V4L2_CID_MUX_CH1_XU_FRAMEINTRVL		(V4L2_CID_XU_BASE + 1)
#define V4L2_CID_MUX_CH1_XU_COMPRESSION_Q	(V4L2_CID_XU_BASE + 2)
#define V4L2_CID_MUX_CH1_XU_GOP_HIERARCHY_LEVEL (V4L2_CID_XU_BASE + 3)
#define	V4L2_CID_MUX_CH1_XU_ZOOM		(V4L2_CID_XU_BASE + 4)
#define V4L2_CID_MUX_CH1_XU_BITRATE		(V4L2_CID_XU_BASE + 5)
#define V4L2_CID_MUX_CH1_XU_FORCE_I_FRAME	(V4L2_CID_XU_BASE + 6)
#define V4L2_CID_MUX_CH1_XU_VUI_ENABLE		(V4L2_CID_XU_BASE + 7)
#define V4L2_CID_MUX_CH1_XU_CHCOUNT		(V4L2_CID_XU_BASE + 8)
#define V4L2_CID_MUX_CH1_XU_CHTYPE		(V4L2_CID_XU_BASE + 9)
#define V4L2_CID_MUX_CH1_XU_GOP_LENGTH		(V4L2_CID_XU_BASE + 10)
#define V4L2_CID_MUX_CH1_XU_AVC_PROFILE		(V4L2_CID_XU_BASE + 11)
#define V4L2_CID_MUX_CH1_XU_AVC_MAX_FRAME_SIZE	(V4L2_CID_XU_BASE + 12)
#define V4L2_CID_MUX_CH1_XU_AVC_LEVEL		(V4L2_CID_XU_BASE + 13)
#define V4L2_CID_MUX_CH1_XU_PIC_TIMING_ENABLE   (V4L2_CID_XU_BASE + 14)
#define V4L2_CID_MUX_CH1_XU_VFLIP		(V4L2_CID_XU_BASE + 15)
#define V4L2_CID_MUX_CH1_XU_AUDIO_BITRATE	(V4L2_CID_XU_BASE + 16)

//mux2 h264/mjpeg
#define V4L2_CID_MUX_CH2_XU_RESOLUTION		(V4L2_CID_XU_BASE + 17)
#define V4L2_CID_MUX_CH2_XU_FRAMEINTRVL		(V4L2_CID_XU_BASE + 18)
#define V4L2_CID_MUX_CH2_XU_PIC_TIMING_ENABLE   (V4L2_CID_XU_BASE + 19)
#define V4L2_CID_MUX_CH2_XU_GOP_HIERARCHY_LEVEL (V4L2_CID_XU_BASE + 20)
#define	V4L2_CID_MUX_CH2_XU_ZOOM		(V4L2_CID_XU_BASE + 21)
#define V4L2_CID_MUX_CH2_XU_BITRATE		(V4L2_CID_XU_BASE + 22)
#define V4L2_CID_MUX_CH2_XU_FORCE_I_FRAME	(V4L2_CID_XU_BASE + 23)
#define V4L2_CID_MUX_CH2_XU_VUI_ENABLE		(V4L2_CID_XU_BASE + 24)
#define V4L2_CID_MUX_CH2_XU_COMPRESSION_Q	(V4L2_CID_XU_BASE + 25)
#define V4L2_CID_MUX_CH2_XU_CHTYPE		(V4L2_CID_XU_BASE + 26)
#define V4L2_CID_MUX_CH2_XU_GOP_LENGTH		(V4L2_CID_XU_BASE + 27)
#define V4L2_CID_MUX_CH2_XU_AVC_PROFILE		(V4L2_CID_XU_BASE + 28)
#define V4L2_CID_MUX_CH2_XU_AVC_MAX_FRAME_SIZE	(V4L2_CID_XU_BASE + 29)
#define V4L2_CID_MUX_CH2_XU_AVC_LEVEL		(V4L2_CID_XU_BASE + 30)

//mux3 h264/mjpeg
#define V4L2_CID_MUX_CH3_XU_RESOLUTION		(V4L2_CID_XU_BASE + 33)
#define V4L2_CID_MUX_CH3_XU_FRAMEINTRVL		(V4L2_CID_XU_BASE + 34)
#define V4L2_CID_MUX_CH3_XU_PIC_TIMING_ENABLE   (V4L2_CID_XU_BASE + 35)
#define V4L2_CID_MUX_CH3_XU_GOP_HIERARCHY_LEVEL (V4L2_CID_XU_BASE + 36)
#define	V4L2_CID_MUX_CH3_XU_ZOOM		(V4L2_CID_XU_BASE + 37)
#define V4L2_CID_MUX_CH3_XU_BITRATE		(V4L2_CID_XU_BASE + 38)
#define V4L2_CID_MUX_CH3_XU_FORCE_I_FRAME	(V4L2_CID_XU_BASE + 39)
#define V4L2_CID_MUX_CH3_XU_VUI_ENABLE		(V4L2_CID_XU_BASE + 40)
#define V4L2_CID_MUX_CH3_XU_COMPRESSION_Q	(V4L2_CID_XU_BASE + 41)
#define V4L2_CID_MUX_CH3_XU_CHTYPE		(V4L2_CID_XU_BASE + 42)
#define V4L2_CID_MUX_CH3_XU_GOP_LENGTH		(V4L2_CID_XU_BASE + 43)
#define V4L2_CID_MUX_CH3_XU_AVC_PROFILE		(V4L2_CID_XU_BASE + 44)
#define V4L2_CID_MUX_CH3_XU_AVC_MAX_FRAME_SIZE	(V4L2_CID_XU_BASE + 45)
#define V4L2_CID_MUX_CH3_XU_AVC_LEVEL		(V4L2_CID_XU_BASE + 46)

//mux4 h264/mjpeg
#define V4L2_CID_MUX_CH4_XU_RESOLUTION		(V4L2_CID_XU_BASE + 47)
#define V4L2_CID_MUX_CH4_XU_FRAMEINTRVL		(V4L2_CID_XU_BASE + 48)
#define V4L2_CID_MUX_CH4_XU_PIC_TIMING_ENABLE   (V4L2_CID_XU_BASE + 49)
#define V4L2_CID_MUX_CH4_XU_GOP_HIERARCHY_LEVEL (V4L2_CID_XU_BASE + 50)
#define	V4L2_CID_MUX_CH4_XU_ZOOM		(V4L2_CID_XU_BASE + 51)
#define V4L2_CID_MUX_CH4_XU_BITRATE		(V4L2_CID_XU_BASE + 52)
#define V4L2_CID_MUX_CH4_XU_FORCE_I_FRAME	(V4L2_CID_XU_BASE + 53)
#define V4L2_CID_MUX_CH4_XU_VUI_ENABLE		(V4L2_CID_XU_BASE + 54)
#define V4L2_CID_MUX_CH4_XU_COMPRESSION_Q	(V4L2_CID_XU_BASE + 55)
#define V4L2_CID_MUX_CH4_XU_CHTYPE		(V4L2_CID_XU_BASE + 56)
#define V4L2_CID_MUX_CH4_XU_GOP_LENGTH		(V4L2_CID_XU_BASE + 57)
#define V4L2_CID_MUX_CH4_XU_AVC_PROFILE		(V4L2_CID_XU_BASE + 58)
#define V4L2_CID_MUX_CH4_XU_AVC_MAX_FRAME_SIZE	(V4L2_CID_XU_BASE + 59)
#define V4L2_CID_MUX_CH4_XU_AVC_LEVEL		(V4L2_CID_XU_BASE + 60)

//mux5
#define V4L2_CID_MUX_CH5_XU_RESOLUTION		(V4L2_CID_XU_BASE + 62)
#define V4L2_CID_MUX_CH5_XU_FRAMEINTRVL		(V4L2_CID_XU_BASE + 63)
#define V4L2_CID_MUX_CH5_XU_PIC_TIMING_ENABLE   (V4L2_CID_XU_BASE + 64)
#define V4L2_CID_MUX_CH5_XU_GOP_HIERARCHY_LEVEL (V4L2_CID_XU_BASE + 65)
#define	V4L2_CID_MUX_CH5_XU_ZOOM		(V4L2_CID_XU_BASE + 66)
#define V4L2_CID_MUX_CH5_XU_BITRATE		(V4L2_CID_XU_BASE + 67)
#define V4L2_CID_MUX_CH5_XU_FORCE_I_FRAME	(V4L2_CID_XU_BASE + 68)
#define V4L2_CID_MUX_CH5_XU_VUI_ENABLE		(V4L2_CID_XU_BASE + 69)
#define V4L2_CID_MUX_CH5_XU_COMPRESSION_Q	(V4L2_CID_XU_BASE + 70)
#define V4L2_CID_MUX_CH5_XU_CHTYPE		(V4L2_CID_XU_BASE + 71)
#define V4L2_CID_MUX_CH5_XU_GOP_LENGTH		(V4L2_CID_XU_BASE + 72)
#define V4L2_CID_MUX_CH5_XU_AVC_PROFILE		(V4L2_CID_XU_BASE + 73)
#define V4L2_CID_MUX_CH5_XU_AVC_MAX_FRAME_SIZE	(V4L2_CID_XU_BASE + 74)
#define V4L2_CID_MUX_CH5_XU_AVC_LEVEL		(V4L2_CID_XU_BASE + 75)
//mux6
#define V4L2_CID_MUX_CH6_XU_RESOLUTION		(V4L2_CID_XU_BASE + 78)
#define V4L2_CID_MUX_CH6_XU_FRAMEINTRVL		(V4L2_CID_XU_BASE + 79)
#define V4L2_CID_MUX_CH6_XU_PIC_TIMING_ENABLE   (V4L2_CID_XU_BASE + 80)
#define V4L2_CID_MUX_CH6_XU_GOP_HIERARCHY_LEVEL (V4L2_CID_XU_BASE + 81)
#define	V4L2_CID_MUX_CH6_XU_ZOOM		(V4L2_CID_XU_BASE + 82)
#define V4L2_CID_MUX_CH6_XU_BITRATE		(V4L2_CID_XU_BASE + 83)
#define V4L2_CID_MUX_CH6_XU_FORCE_I_FRAME	(V4L2_CID_XU_BASE + 84)
#define V4L2_CID_MUX_CH6_XU_VUI_ENABLE		(V4L2_CID_XU_BASE + 85)
#define V4L2_CID_MUX_CH6_XU_COMPRESSION_Q	(V4L2_CID_XU_BASE + 86)
#define V4L2_CID_MUX_CH6_XU_CHTYPE		(V4L2_CID_XU_BASE + 87)
#define V4L2_CID_MUX_CH6_XU_GOP_LENGTH		(V4L2_CID_XU_BASE + 88)
#define V4L2_CID_MUX_CH6_XU_AVC_LEVEL		(V4L2_CID_XU_BASE + 89)

//mux7
#define V4L2_CID_MUX_CH7_XU_RESOLUTION		(V4L2_CID_XU_BASE + 95)
#define V4L2_CID_MUX_CH7_XU_FRAMEINTRVL		(V4L2_CID_XU_BASE + 96)
#define V4L2_CID_MUX_CH7_XU_PIC_TIMING_ENABLE   (V4L2_CID_XU_BASE + 97)
#define V4L2_CID_MUX_CH7_XU_GOP_HIERARCHY_LEVEL (V4L2_CID_XU_BASE + 98)
#define	V4L2_CID_MUX_CH7_XU_ZOOM		(V4L2_CID_XU_BASE + 99)
#define V4L2_CID_MUX_CH7_XU_BITRATE		(V4L2_CID_XU_BASE + 100)
#define V4L2_CID_MUX_CH7_XU_FORCE_I_FRAME	(V4L2_CID_XU_BASE + 101)
#define V4L2_CID_MUX_CH7_XU_VUI_ENABLE		(V4L2_CID_XU_BASE + 102)
#define V4L2_CID_MUX_CH7_XU_COMPRESSION_Q	(V4L2_CID_XU_BASE + 103)
#define V4L2_CID_MUX_CH7_XU_CHTYPE		(V4L2_CID_XU_BASE + 104)
#define V4L2_CID_MUX_CH7_XU_GOP_LENGTH		(V4L2_CID_XU_BASE + 105)
#define V4L2_CID_MUX_CH7_XU_AVC_LEVEL		(V4L2_CID_XU_BASE + 106)

//common mux xu
#define V4L2_CID_MUX_XU_START_CHANNEL		(V4L2_CID_XU_BASE + 107)
#define V4L2_CID_MUX_XU_STOP_CHANNEL		(V4L2_CID_XU_BASE + 108)

#define V4L2_CID_PU_XU_ANF_ENABLE          (V4L2_CID_XU_BASE + 110)
#define V4L2_CID_PU_XU_NF_STRENGTH         (V4L2_CID_XU_BASE + 111)
#define V4L2_CID_PU_XU_TF_STRENGTH         (V4L2_CID_XU_BASE + 112)
#define V4L2_CID_PU_XU_ADAPTIVE_WDR_ENABLE (V4L2_CID_XU_BASE + 113)
#define V4L2_CID_PU_XU_WDR_STRENGTH        (V4L2_CID_XU_BASE + 114)
#define V4L2_CID_PU_XU_AUTO_EXPOSURE       (V4L2_CID_XU_BASE + 115)
#define V4L2_CID_PU_XU_EXPOSURE_TIME       (V4L2_CID_XU_BASE + 116)
#define V4L2_CID_PU_XU_AUTO_WHITE_BAL      (V4L2_CID_XU_BASE + 117)
#define V4L2_CID_PU_XU_WHITE_BAL_TEMP      (V4L2_CID_XU_BASE + 118)
#define V4L2_CID_PU_XU_VFLIP               (V4L2_CID_XU_BASE + 119)
#define V4L2_CID_PU_XU_HFLIP               (V4L2_CID_XU_BASE + 120)
#define V4L2_CID_PU_XU_WB_ZONE_SEL_ENABLE  (V4L2_CID_XU_BASE + 121)
#define V4L2_CID_PU_XU_WB_ZONE_SEL         (V4L2_CID_XU_BASE + 122)
#define V4L2_CID_PU_XU_EXP_ZONE_SEL_ENABLE (V4L2_CID_XU_BASE + 123)
#define V4L2_CID_PU_XU_EXP_ZONE_SEL        (V4L2_CID_XU_BASE + 124)
#define V4L2_CID_PU_XU_MAX_ANALOG_GAIN     (V4L2_CID_XU_BASE + 125)
#define V4L2_CID_PU_XU_HISTO_EQ            (V4L2_CID_XU_BASE + 126)
#define V4L2_CID_PU_XU_SHARPEN_FILTER      (V4L2_CID_XU_BASE + 127)
#define V4L2_CID_PU_XU_GAIN_MULTIPLIER     (V4L2_CID_XU_BASE + 128)
#define V4L2_CID_PU_XU_CROP                (V4L2_CID_XU_BASE + 129)
#define V4L2_CID_PU_XU_CROP_ENABLE         (V4L2_CID_XU_BASE + 129)
#define V4L2_CID_PU_XU_CROP_WIDTH          (V4L2_CID_XU_BASE + 130)
#define V4L2_CID_PU_XU_CROP_HEIGHT         (V4L2_CID_XU_BASE + 131)
#define V4L2_CID_PU_XU_CROP_X              (V4L2_CID_XU_BASE + 132)
#define V4L2_CID_PU_XU_CROP_Y              (V4L2_CID_XU_BASE + 133)
#define V4L2_CID_PU_XU_EXP_MIN_FR_RATE     (V4L2_CID_XU_BASE + 134)

#define V4L2_CID_XU_END                    (V4L2_CID_XU_BASE + 135)

/* GUIDs */
/* {303B461D-BC63-44c3-8230-6741CAEB5D77} */
#define GUID_VIDCAP_EXT \
	{0x1d, 0x46, 0x3b, 0x30, 0x63, 0xbc, 0xc3, 0x44,\
	 0x82, 0x30, 0x67, 0x41, 0xca, 0xeb, 0x5d, 0x77}
/* {6DF18A70-C113-428e-88C5-4AFF0E286AAA} */
#define GUID_VIDENC_EXT \
	{0x70, 0x8a, 0xf1, 0x6d, 0x13, 0xc1, 0x8e, 0x42,\
	 0x88, 0xc5, 0x4a, 0xff, 0x0e, 0x28, 0x6a, 0xaa}
/* {ba2b92d9-26f2-4294-ae42-06dd684debe4} */
#define AVC_XU_GUID \
	{0xd9, 0x92, 0x2b, 0xba, 0xf2, 0x26, 0x94, 0x42,\
	 0x42, 0xae, 0xe4, 0xeb, 0x4d, 0x68, 0xdd, 0x06}
#define MUX1_XU_GUID AVC_XU_GUID
#define MUX2_XU_GUID \
	{0xd9, 0x92, 0x2b, 0xbb, 0xf2, 0x26, 0x94, 0x42,\
	 0x42, 0xae, 0xe4, 0xeb, 0x4d, 0x68, 0xdd, 0x06}
#define MUX3_XU_GUID \
	{0xd9, 0x92, 0x2b, 0xbc, 0xf2, 0x26, 0x94, 0x42,\
	 0x42, 0xae, 0xe4, 0xeb, 0x4d, 0x68, 0xdd, 0x06}
#define MUX4_XU_GUID \
	{0xd9, 0x92, 0x2b, 0xbd, 0xf2, 0x26, 0x94, 0x42,\
	 0x42, 0xae, 0xe4, 0xeb, 0x4d, 0x68, 0xdd, 0x06}
#define MUX5_XU_GUID \
	{0xd9, 0x92, 0x2b, 0xbe, 0xf2, 0x26, 0x94, 0x42,\
	 0x42, 0xae, 0xe4, 0xeb, 0x4d, 0x68, 0xdd, 0x06}
#define MUX6_XU_GUID \
	{0xd9, 0x92, 0x2b, 0xbf, 0xf2, 0x26, 0x94, 0x42,\
	 0x42, 0xae, 0xe4, 0xeb, 0x4d, 0x68, 0xdd, 0x06}
#define MUX7_XU_GUID \
	{0xd9, 0x92, 0x2b, 0xc0, 0xf2, 0x26, 0x94, 0x42,\
	 0x42, 0xae, 0xe4, 0xeb, 0x4d, 0x68, 0xdd, 0x06}

#define PU_XU_GUID \
	{0x12, 0xcd, 0x5d, 0xdf, 0x5f, 0x7d, 0xba, 0x4b,\
	 0xbb, 0x6d, 0x4b, 0x62, 0x5a, 0xdd, 0x52, 0x72}

/* {bd5321b4-d635-ca45-b203-4e0149b301bc} */
#define SKYPE_XU_GUID \
	{0xb4, 0x21, 0x53, 0xbd, 0x35, 0xd6, 0x45, 0xca, \
	 0xb2, 0x03, 0x4e, 0x01, 0x49, 0xb3, 0x01, 0xbc}

#define UVC_GUID_UVC_CAMERA \
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}
#define UVC_GUID_UVC_PROCESSING \
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01}

#define CT_PANTILT_ABSOLUTE_CONTROL     0x0d
#define PU_DIGITAL_MULTIPLIER_CONTROL   0x0e

#define V4L2_CID_DIGITIAL_MULTIPLIER	(V4L2_CID_CAMERA_CLASS_BASE+32)

// framerate in 100 nsec units //TBD correctly
#define FRI(x)	((float)10000000/x) //TBD
#define FRR(x)	((float)10000000/(x+0.5))

enum OLD_XU_CTRL
{
    OLD_XU_BITRATE = 1,
    OLD_XU_AVC_PROFILE,
    OLD_XU_AVC_LEVEL,
    OLD_XU_PICTURE_CODING,
    OLD_XU_GOP_STRUCTURE,
    OLD_XU_GOP_LENGTH,
    OLD_XU_FRAME_RATE,
    OLD_XU_RESOLUTION,

    OLD_XU_AVMUX_ENABLE,
    OLD_XU_AUD_BIT_RATE,
    OLD_XU_SAMPLE_RATE,
    OLD_XU_NUM_CHAN,
    OLD_XU_CAP_STOP,
    OLD_XU_CAP_QUERY_EOS,

    OLD_XU_FORCE_I_FRAME,
    OLD_XU_GET_VERSION,
};

enum AVC_XU_CTRL {
	AVC_XU_PROFILE = 1,
	AVC_XU_LEVEL,
	AVC_XU_PICTURE_CODING,
	AVC_XU_RESOLUTION,
	AVC_XU_GOP_STRUCTURE,
	AVC_XU_GOP_LENGTH,
	AVC_XU_BITRATE,
	AVC_XU_FORCE_I_FRAME,
	AVC_XU_MAX_NAL,
	AVC_XU_VUI_ENABLE,
	AVC_XU_PIC_TIMING_ENABLE,
	AVC_XU_GOP_HIERARCHY_LEVEL,
	AVC_XU_AV_MUX_ENABLE,
	AVC_XU_MAX_FRAME_SIZE,
};

enum SKYPE_XU_CTRL {
	SKYPE_XU_VERSION = 1,
	SKYPE_XU_LASTERROR,
	SKYPE_XU_FIRMWAREDAYS,
	SKYPE_XU_STREAMID,
	SKYPE_XU_ENDPOINT_SETTING = 5,

	SKYPE_XU_STREAMFORMATPROBE = 8,
	SKYPE_XU_STREAMFORMATCOMMIT,
	SKYPE_XU_STREAMFORMATPROBE_TYPE,
	SKYPE_XU_STREAMFORMATPROBE_WIDTH,
	SKYPE_XU_STREAMFORMATPROBE_HEIGHT,
	SKYPE_XU_STREAMFORMATPROBE_FRAME_INTERVAL = 14,

	SKYPE_XU_BITRATE = 24,
	SKYPE_XU_FRAMEINTERVAL,
	SKYPE_XU_GENERATEKEYFRAME = 26,

	SKYPE_XU_NUM_CTRLS = 32,
};

enum MUX_XU_CTRL {
	//for all video channels
	MUX_XU_RESOLUTION = 1,
	MUX_XU_FRAMEINTRVL,
	MUX_XU_PIC_TIMING_ENABLE,
	MUX_XU_GOP_HIERARCHY_LEVEL,
	MUX_XU_ZOOM,

	//for H264 based channels only
	MUX_XU_BITRATE,
	MUX_XU_FORCE_I_FRAME,
	MUX_XU_VUI_ENABLE,
	MUX_XU_AVC_LEVEL,

	//for MJPEG channel only
	MUX_XU_COMPRESSION_Q,

	MUX_XU_CHCOUNT,
	MUX_XU_CHTYPE,
	MUX_XU_GOP_LENGTH,
	MUX_XU_AVC_PROFILE,
	MUX_XU_AVC_MAX_FRAME_SIZE,
	MUX_XU_START_CHANNEL,
	MUX_XU_STOP_CHANNEL,
	MUX_XU_HFLIP,
	MUX_XU_VFLIP,
	
	//Audio bitrate
	MUX_XU_AUDIO_BITRATE,

	MUX_XU_NUM_CTRLS,
};

/* Controls in the XU */
enum PU_XU_CTRL {
	PU_XU_ANF_ENABLE = 1,
	PU_XU_NF_STRENGTH,
	PU_XU_TF_STRENGTH,
	PU_XU_ADAPTIVE_WDR_ENABLE,
	PU_XU_WDR_STRENGTH,
	PU_XU_AE_ENABLE,
	PU_XU_EXPOSURE_TIME,
	PU_XU_AWB_ENABLE,
	PU_XU_WB_TEMPERATURE,
	PU_XU_VFLIP,
	PU_XU_HFLIP,
	PU_XU_WB_ZONE_SEL_ENABLE,
	PU_XU_WB_ZONE_SEL,
	PU_XU_EXP_ZONE_SEL_ENABLE,
	PU_XU_EXP_ZONE_SEL,
	PU_XU_MAX_ANALOG_GAIN,
	PU_XU_HISTO_EQ,
	PU_XU_SHARPEN_FILTER,
	PU_XU_GAIN_MULTIPLIER,
	PU_XU_CROP,
	PU_XU_EXP_MIN_FR_RATE,
};


struct uvc_xu_data {
	__u8	entity[16];	/* Extension unit GUID */
	__u8	selector;	/* UVC control selector */
	__u8	size;		/* V4L2 control size (in bits) */
	__u8	offset;		/* V4L2 control offset (in bits) */
	__u32	id;		/* V4L2 control identifier */
	__u8	name[32];	/* V4L2 control name */
};

/* Stream format for SECS */
struct StreamFormat {
	__u8 bStreamType;	/* As per StreamFormat listing. */
	__u16 wWidth;		/* Frame width in pixels. */
	__u16 wHeight;		/* Frame height in pixels. */
	__u32 dwFrameInterval;	/* Frame interval in 100 ns units */
	__u32 dwBitrate;	/* Bitrate in bits per second */
} __attribute__ ((packed));

/* Mapping of the resolutions to their index in the extension control */
#define R(w,h) (((w)<<16)|(h))
#define NUM_XU_RES 16
struct mapping {
	char    name[9];
	int     id;
	int     id2;
};
struct mapping xu_res_mapping[NUM_XU_RES] = {
	{"160x120", 0,  R(160,120)},
	{"176x144", 1,  R(176,144)},
	{"256x144", 9,  R(256,144)},
	{"320x240", 2,  R(320,240)},
	{"352x288", 3,  R(352,288)},
	{"368x208", 10, R(368,208)},
	{"384x240", 4,  R(384,240)},
	{"432x240", 15, R(432,240)},
	{"480x272", 11, R(480,272)},
	{"624x352", 12, R(624,352)},
	{"640x480", 5,  R(640,480)},
	{"720x480", 6,  R(720,480)},
	{"704x576", 7,  R(704,576)},
	{"912x512", 13, R(912,512)},
	{"960x720", 14, R(960,720)},
	{"1280x720", 8, R(1280,720)},
};

/* Mapping between mxuvc video format IDs and SECS format IDs */
static int secs_format_mapping[NUM_VID_FORMAT] = {
		[0 ... (NUM_VID_FORMAT-1)] = -1,
		/* Only overwrite the formats that have a match */
		[VID_FORMAT_H264_RAW] = F_AVC,
		[VID_FORMAT_MJPEG_RAW] = F_MJPEG,
		[VID_FORMAT_YUY2_RAW] = F_YUY2,
		[VID_FORMAT_NV12_RAW] = F_NV12,
};

#define DEV_OFFSET_DEFAULT 0

int ipcam_mode = 0;
static unsigned int buffer_count = BUFFER_COUNT;
unsigned int mux_channel_count = 1; //minimum mux channel count has to be one else CH1 init logic will fail.

struct buffer_info {
	void   *start;
	size_t  length;
};
static struct video_stream {
	int fd;
	int enabled;
	int started;

	/* V4l2 buffer/settings */
	struct buffer_info *buffers;
	unsigned int n_buffers;
	struct v4l2_format fmt;

	/* Callback */
	mxuvc_video_cb_t cb;
	void *cb_user_data;

	/* Current stream format settings.
	 * Those settings are cached in order to populate the SECS
	 * StreamFormat with the current values when doing a StreamFormatProbe */
	video_format_t cur_vfmt;
	uint16_t cur_width;
	uint16_t cur_height;
	uint32_t cur_bitrate;
	uint32_t cur_framerate;

	/* Captured frame count */
	unsigned int frame_count;

	/* AAC muxing enabled ? */
	unsigned int mux_aac_ts;

	/* SECS */
	unsigned int secs_supported; /* SECS mode supported but not enabled */
	unsigned int secs_enabled; /* SECS mode enabled */

} *video_stream;

static struct {
	int fd;
	int enabled;
	int started;
	int waiting;
	int ref_count;
	/* V4l2 buffer/settings */
	struct buffer_info *buffers;
	unsigned int n_buffers;
} mux_stream;

static int getset_mux_channel_param(video_channel_t ch, void *data,
				uint32_t data_size,
				enum MUX_XU_CTRL xu_name,
				uint32_t set);
static int set_pan(video_channel_t ch, int32_t pan);
static int get_pan(video_channel_t ch, int32_t *pan);
static int set_tilt(video_channel_t ch, int32_t tilt);
static int get_tilt(video_channel_t ch, int32_t *tilt);

static int is_mux_stream(struct video_stream *vstream)
{
	if (ipcam_mode && (vstream->fd == mux_stream.fd))
		return 1;

	return 0;
}

static int xioctl(int fh, int request, void *arg)
{
	int r;

	do {
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

static int vidformat2fourcc(video_format_t fmt, uint32_t *fourcc)
{
	switch(fmt) {
	case VID_FORMAT_H264_RAW:
		*fourcc = v4l2_fourcc('H','2','6','4');
		return 0;
	case VID_FORMAT_H264_TS:
		*fourcc = V4L2_PIX_FMT_MPEG;
		return 0;
	case VID_FORMAT_H264_AAC_TS:
		*fourcc = V4L2_PIX_FMT_MPEG;
		return 0;
	case VID_FORMAT_MJPEG_RAW:
		*fourcc = v4l2_fourcc('M','J','P','G');
		return 0;
	case VID_FORMAT_YUY2_RAW:
		*fourcc = v4l2_fourcc('Y','U','Y','V');
		return 0;
	case VID_FORMAT_NV12_RAW:
		*fourcc = v4l2_fourcc('N','V','1','2');
		return 0;
	case VID_FORMAT_GREY_RAW:
		*fourcc = V4L2_PIX_FMT_GREY;
		return 0;
	case VID_FORMAT_MUX:
		*fourcc = v4l2_fourcc('M','U','X',' ');
		return 0;
	default:
		ERROR(-1, "Unknown video format %i.", fmt);
	}
}

static int fourcc2vidformat(uint32_t fourcc, video_format_t *fmt)
{
	int f;
	uint32_t fcc;
	for (f=FIRST_VID_FORMAT; f<NUM_VID_FORMAT; f++) {
		vidformat2fourcc(f, &fcc);
		if (fcc == fourcc) {
			*fmt = f;
			return 0;
		}
	}

	ERROR(-1, "Unknown fourcc %i\n", fourcc);
}


/* FIXME - update uvc driver instead */
static struct uvc_xu_control_mapping_old mappings[] = {
	{V4L2_CID_PAN_ABSOLUTE, "Pan, Absolute",
		UVC_GUID_UVC_CAMERA, CT_PANTILT_ABSOLUTE_CONTROL,
		32, 0, V4L2_CTRL_TYPE_INTEGER, UVC_CTRL_DATA_TYPE_SIGNED},

	{V4L2_CID_TILT_ABSOLUTE, "Tilt, Absolute",
		UVC_GUID_UVC_CAMERA, CT_PANTILT_ABSOLUTE_CONTROL,
		32, 32, V4L2_CTRL_TYPE_INTEGER, UVC_CTRL_DATA_TYPE_SIGNED},

	{V4L2_CID_DIGITIAL_MULTIPLIER, "Digital Multiplier",
		UVC_GUID_UVC_PROCESSING, PU_DIGITAL_MULTIPLIER_CONTROL,
		16, 0, V4L2_CTRL_TYPE_INTEGER, UVC_CTRL_DATA_TYPE_SIGNED},
};
static struct uvc_xu_data skype_xu_data[] = {
    {GUID_VIDCAP_EXT, OLD_XU_BITRATE,        32,  0, V4L2_CID_XU_BITRATE,        "Bitrate"},
    {GUID_VIDCAP_EXT, OLD_XU_AVC_PROFILE,    32,  0, V4L2_CID_XU_AVC_PROFILE,    "Profile"},
    {GUID_VIDCAP_EXT, OLD_XU_AVC_LEVEL,      32,  0, V4L2_CID_XU_AVC_LEVEL,      "Level"},
    {GUID_VIDCAP_EXT, OLD_XU_PICTURE_CODING, 32,  0, V4L2_CID_XU_PICTURE_CODING, "Picture Coding"},
    {GUID_VIDCAP_EXT, OLD_XU_GOP_STRUCTURE,  32,  0, V4L2_CID_XU_GOP_STRUCTURE,  "GOP Structure"},
    {GUID_VIDCAP_EXT, OLD_XU_GOP_LENGTH,     32,  0, V4L2_CID_XU_GOP_LENGTH,     "GOP Length"},
    {GUID_VIDCAP_EXT, OLD_XU_RESOLUTION,     32,  0, V4L2_CID_XU_RESOLUTION,     "Resolution"},
    {GUID_VIDCAP_EXT, OLD_XU_FORCE_I_FRAME,  32,  0, V4L2_CID_XU_FORCE_I_FRAME,  "Force I Frame"},
    {GUID_VIDCAP_EXT, OLD_XU_GET_VERSION,    32,  0, V4L2_CID_XU_GET_VERSION,    "Version"},

    {GUID_VIDENC_EXT, OLD_XU_BITRATE,        32,  0, V4L2_CID_XU_BITRATE,        "Bitrate"},
    {GUID_VIDENC_EXT, OLD_XU_AVC_PROFILE,    32,  0, V4L2_CID_XU_AVC_PROFILE,    "Profile"},
    {GUID_VIDENC_EXT, OLD_XU_AVC_LEVEL,      32,  0, V4L2_CID_XU_AVC_LEVEL,      "Level"},
    {GUID_VIDENC_EXT, OLD_XU_PICTURE_CODING, 32,  0, V4L2_CID_XU_PICTURE_CODING, "Picture Coding"},
    {GUID_VIDENC_EXT, OLD_XU_GOP_STRUCTURE,  32,  0, V4L2_CID_XU_GOP_STRUCTURE,  "GOP Structure"},
    {GUID_VIDENC_EXT, OLD_XU_GOP_LENGTH,     32,  0, V4L2_CID_XU_GOP_LENGTH,     "GOP Length"},
    {GUID_VIDENC_EXT, OLD_XU_RESOLUTION,     32,  0, V4L2_CID_XU_RESOLUTION,     "Resolution"},
    {GUID_VIDENC_EXT, OLD_XU_FORCE_I_FRAME,  32,  0, V4L2_CID_XU_FORCE_I_FRAME,  "Force I Frame"},
    {GUID_VIDENC_EXT, OLD_XU_GET_VERSION,    32,  0, V4L2_CID_XU_GET_VERSION,    "Version"},

    {AVC_XU_GUID, AVC_XU_PROFILE,           32,  0,  V4L2_CID_XU_AVC_PROFILE,      "Profile"},
    {AVC_XU_GUID, AVC_XU_LEVEL,             32,  0,  V4L2_CID_XU_AVC_LEVEL,        "Level"},
    {AVC_XU_GUID, AVC_XU_PICTURE_CODING,    32,  0,  V4L2_CID_XU_PICTURE_CODING,   "Picture Coding"},
    {AVC_XU_GUID, AVC_XU_RESOLUTION,        32,  0,  V4L2_CID_XU_RESOLUTION2,      "Resolution"},
    {AVC_XU_GUID, AVC_XU_GOP_STRUCTURE,     32,  0,  V4L2_CID_XU_GOP_STRUCTURE,    "GOP Structure"},
    {AVC_XU_GUID, AVC_XU_GOP_LENGTH,        32,  0,  V4L2_CID_XU_GOP_LENGTH,       "GOP Length"},
    {AVC_XU_GUID, AVC_XU_BITRATE,           32,  0,  V4L2_CID_XU_BITRATE,          "Bitrate"},
    {AVC_XU_GUID, AVC_XU_FORCE_I_FRAME,     32,  0,  V4L2_CID_XU_FORCE_I_FRAME,    "Force I Frame"},
    {AVC_XU_GUID, AVC_XU_MAX_NAL,           32,  0,  V4L2_CID_XU_MAX_NAL,          "Max NAL Units"},
    {AVC_XU_GUID, AVC_XU_VUI_ENABLE,        32,  0,  V4L2_CID_XU_VUI_ENABLE,       "VUI Enable"},
    {AVC_XU_GUID, AVC_XU_PIC_TIMING_ENABLE, 32,  0,  V4L2_CID_XU_PIC_TIMING_ENABLE,"Pic Timing Enable"},
    {AVC_XU_GUID, AVC_XU_AV_MUX_ENABLE,     32,  0,  V4L2_CID_XU_AV_MUX_ENABLE,    "AV Mux Enable"},
    {AVC_XU_GUID, AVC_XU_MAX_FRAME_SIZE,    32,  0,  V4L2_CID_XU_MAX_FRAME_SIZE,   "Max Frame Size"},

    {SKYPE_XU_GUID, SKYPE_XU_VERSION,             8,  0,  V4L2_CID_SKYPE_XU_VERSION,            "Version"},
    {SKYPE_XU_GUID, SKYPE_XU_LASTERROR,           8,  0,  V4L2_CID_SKYPE_XU_LASTERROR,          "Last Error"},
    {SKYPE_XU_GUID, SKYPE_XU_FIRMWAREDAYS,       16,  0,  V4L2_CID_SKYPE_XU_FIRMWAREDAYS,       "Firmware Days"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMID,            8,  0,  V4L2_CID_SKYPE_XU_STREAMID,           "StreamID"},
    {SKYPE_XU_GUID, SKYPE_XU_ENDPOINT_SETTING,    8,  0,  V4L2_CID_SKYPE_XU_ENDPOINT_SETTING,   "Endpoint Setting"},

    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATPROBE,   8,  0,  V4L2_CID_SKYPE_XU_STREAMFORMATPROBE,   "Probe - Stream Type"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATPROBE,  16,  8,  V4L2_CID_SKYPE_XU_STREAMFORMATPROBE+1, "Probe - Width"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATPROBE,  16, 24,  V4L2_CID_SKYPE_XU_STREAMFORMATPROBE+2, "Probe - Height"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATPROBE,  32, 40,  V4L2_CID_SKYPE_XU_STREAMFORMATPROBE+3, "Probe - Frame Interval"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATPROBE,  32, 72,  V4L2_CID_SKYPE_XU_STREAMFORMATPROBE+4, "Probe - Bitrate"},

    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATCOMMIT,  8,  0, V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT,   "Commit - Stream Type"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATCOMMIT, 16,  8, V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT+1, "Commit - Width"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATCOMMIT, 16, 24, V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT+2, "Commit - Height"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATCOMMIT, 32, 40, V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT+3, "Commit - Frame Interval"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATCOMMIT, 32, 72, V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT+4, "Commit - Bitrate"},

    {SKYPE_XU_GUID, SKYPE_XU_BITRATE,            32,  0,  V4L2_CID_SKYPE_XU_BITRATE,            "Bitrate"},
    {SKYPE_XU_GUID, SKYPE_XU_FRAMEINTERVAL,      32,  0,  V4L2_CID_SKYPE_XU_FRAMEINTERVAL,      "Frame Interval"},
    {SKYPE_XU_GUID, SKYPE_XU_GENERATEKEYFRAME,    8,  0,  V4L2_CID_SKYPE_XU_GENERATEKEYFRAME,   "Generate Key Frame"},


    {PU_XU_GUID, PU_XU_ADAPTIVE_WDR_ENABLE, 32,  0,  V4L2_CID_PU_XU_ADAPTIVE_WDR_ENABLE,"Adaptive WDR Enable"},
    {PU_XU_GUID, PU_XU_WDR_STRENGTH,        32,  0,  V4L2_CID_PU_XU_WDR_STRENGTH,       "WDR Strength"},
    {PU_XU_GUID, PU_XU_AE_ENABLE,           32,  0,  V4L2_CID_PU_XU_AUTO_EXPOSURE,      "Auto Exposure"},
    {PU_XU_GUID, PU_XU_EXPOSURE_TIME,       32,  0,  V4L2_CID_PU_XU_EXPOSURE_TIME,      "Exposure Time"},
    {PU_XU_GUID, PU_XU_AWB_ENABLE,          32,  0,  V4L2_CID_PU_XU_AUTO_WHITE_BAL,     "Auto White Balance"},
    {PU_XU_GUID, PU_XU_WB_TEMPERATURE,      32,  0,  V4L2_CID_PU_XU_WHITE_BAL_TEMP,     "White Balance Temperature"},
    {PU_XU_GUID, PU_XU_VFLIP,               32,  0,  V4L2_CID_PU_XU_VFLIP,              "Vertical Flip"},
    {PU_XU_GUID, PU_XU_HFLIP,               32,  0,  V4L2_CID_PU_XU_HFLIP,              "Horizontal Flip"},
    {PU_XU_GUID, PU_XU_WB_ZONE_SEL_ENABLE,  32,  0,  V4L2_CID_PU_XU_WB_ZONE_SEL_ENABLE, "White Balance Zone Select"},
    {PU_XU_GUID, PU_XU_WB_ZONE_SEL,         32,  0,  V4L2_CID_PU_XU_WB_ZONE_SEL,        "White Balance Zone"},
    {PU_XU_GUID, PU_XU_EXP_ZONE_SEL_ENABLE, 32,  0,  V4L2_CID_PU_XU_EXP_ZONE_SEL_ENABLE,"Exposure Zone Select"},
    {PU_XU_GUID, PU_XU_EXP_ZONE_SEL,        32,  0,  V4L2_CID_PU_XU_EXP_ZONE_SEL,       "Exposure Zone"},
    {PU_XU_GUID, PU_XU_MAX_ANALOG_GAIN,     32,  0,  V4L2_CID_PU_XU_MAX_ANALOG_GAIN,    "Max Analog Gain"},
    {PU_XU_GUID, PU_XU_HISTO_EQ,            32,  0,  V4L2_CID_PU_XU_HISTO_EQ,           "Histogram Equalization"},
    {PU_XU_GUID, PU_XU_SHARPEN_FILTER,      32,  0,  V4L2_CID_PU_XU_SHARPEN_FILTER,     "Sharpen Filter Level"},
    {PU_XU_GUID, PU_XU_GAIN_MULTIPLIER,     32,  0,  V4L2_CID_PU_XU_GAIN_MULTIPLIER,    "Exposure Gain Multiplier"},
    /* Crop mode */
    {PU_XU_GUID, PU_XU_CROP,                16,  0,  V4L2_CID_PU_XU_CROP_ENABLE    , "Crop Enable"},
    {PU_XU_GUID, PU_XU_CROP,                16,  16, V4L2_CID_PU_XU_CROP_WIDTH     , "Crop Width"},
    {PU_XU_GUID, PU_XU_CROP,                16,  32, V4L2_CID_PU_XU_CROP_HEIGHT    , "Crop Height"},
    {PU_XU_GUID, PU_XU_CROP,                16,  48, V4L2_CID_PU_XU_CROP_X         , "Crop X"},
    {PU_XU_GUID, PU_XU_CROP,                16,  64, V4L2_CID_PU_XU_CROP_Y         , "Crop Y"},

    {PU_XU_GUID, PU_XU_EXP_MIN_FR_RATE,     32,  0,  V4L2_CID_PU_XU_EXP_MIN_FR_RATE, "Minimum AE Frame Rate"},
};

static struct uvc_xu_data mux_xu_data[] = {
    /*{GUID_VIDCAP_EXT, OLD_XU_BITRATE,        32,  0, V4L2_CID_XU_BITRATE,        "Bitrate"},
    {GUID_VIDCAP_EXT, OLD_XU_AVC_PROFILE,    32,  0, V4L2_CID_XU_AVC_PROFILE,    "Profile"},
    {GUID_VIDCAP_EXT, OLD_XU_AVC_LEVEL,      32,  0, V4L2_CID_XU_AVC_LEVEL,      "Level"},
    {GUID_VIDCAP_EXT, OLD_XU_PICTURE_CODING, 32,  0, V4L2_CID_XU_PICTURE_CODING, "Picture Coding"},
    {GUID_VIDCAP_EXT, OLD_XU_GOP_STRUCTURE,  32,  0, V4L2_CID_XU_GOP_STRUCTURE,  "GOP Structure"},
    {GUID_VIDCAP_EXT, OLD_XU_GOP_LENGTH,     32,  0, V4L2_CID_XU_GOP_LENGTH,     "GOP Length"},
    {GUID_VIDCAP_EXT, OLD_XU_RESOLUTION,     32,  0, V4L2_CID_XU_RESOLUTION,     "Resolution"},
    {GUID_VIDCAP_EXT, OLD_XU_FORCE_I_FRAME,  32,  0, V4L2_CID_XU_FORCE_I_FRAME,  "Force I Frame"},
    {GUID_VIDCAP_EXT, OLD_XU_GET_VERSION,    32,  0, V4L2_CID_XU_GET_VERSION,    "Version"},

    {GUID_VIDENC_EXT, OLD_XU_BITRATE,        32,  0, V4L2_CID_XU_BITRATE,        "Bitrate"},
    {GUID_VIDENC_EXT, OLD_XU_AVC_PROFILE,    32,  0, V4L2_CID_XU_AVC_PROFILE,    "Profile"},
    {GUID_VIDENC_EXT, OLD_XU_AVC_LEVEL,      32,  0, V4L2_CID_XU_AVC_LEVEL,      "Level"},
    {GUID_VIDENC_EXT, OLD_XU_PICTURE_CODING, 32,  0, V4L2_CID_XU_PICTURE_CODING, "Picture Coding"},
    {GUID_VIDENC_EXT, OLD_XU_GOP_STRUCTURE,  32,  0, V4L2_CID_XU_GOP_STRUCTURE,  "GOP Structure"},
    {GUID_VIDENC_EXT, OLD_XU_GOP_LENGTH,     32,  0, V4L2_CID_XU_GOP_LENGTH,     "GOP Length"},
    {GUID_VIDENC_EXT, OLD_XU_RESOLUTION,     32,  0, V4L2_CID_XU_RESOLUTION,     "Resolution"},
    {GUID_VIDENC_EXT, OLD_XU_FORCE_I_FRAME,  32,  0, V4L2_CID_XU_FORCE_I_FRAME,  "Force I Frame"},
    {GUID_VIDENC_EXT, OLD_XU_GET_VERSION,    32,  0, V4L2_CID_XU_GET_VERSION,    "Version"},*/

    {MUX1_XU_GUID, MUX_XU_RESOLUTION,        32,  0,  V4L2_CID_MUX_CH1_XU_RESOLUTION,      "Resolution"},
    {MUX1_XU_GUID, MUX_XU_FRAMEINTRVL,       32,  0,  V4L2_CID_MUX_CH1_XU_FRAMEINTRVL,     "frame interval"},
    {MUX1_XU_GUID, MUX_XU_COMPRESSION_Q,     32,  0,  V4L2_CID_MUX_CH1_XU_COMPRESSION_Q,   "compression quality"},
    {MUX1_XU_GUID, MUX_XU_GOP_HIERARCHY_LEVEL,32, 0,  V4L2_CID_MUX_CH1_XU_GOP_HIERARCHY_LEVEL, "Gop hierarchy level"},
    {MUX1_XU_GUID, MUX_XU_ZOOM,     	     32,  0,  V4L2_CID_MUX_CH1_XU_ZOOM,    	   "Zoom"},
    {MUX1_XU_GUID, MUX_XU_BITRATE,           32,  0,  V4L2_CID_MUX_CH1_XU_BITRATE,         "Bitrate"},
    {MUX1_XU_GUID, MUX_XU_FORCE_I_FRAME,     32,  0,  V4L2_CID_MUX_CH1_XU_FORCE_I_FRAME,   "Force I Frame"},
    {MUX1_XU_GUID, MUX_XU_VUI_ENABLE,        32,  0,  V4L2_CID_MUX_CH1_XU_VUI_ENABLE,       "VUI Enable"},
    {MUX1_XU_GUID, MUX_XU_CHCOUNT,	     32,  0,  V4L2_CID_MUX_CH1_XU_CHCOUNT,     	   "mux channel count"},
    {MUX1_XU_GUID, MUX_XU_CHTYPE,	     32,  0,  V4L2_CID_MUX_CH1_XU_CHTYPE,     	   "channel type"},
    {MUX1_XU_GUID, MUX_XU_GOP_LENGTH,	     32,  0,  V4L2_CID_MUX_CH1_XU_GOP_LENGTH,      "GOP Length"},    
    {MUX1_XU_GUID, MUX_XU_AVC_PROFILE,	     32,  0,  V4L2_CID_MUX_CH1_XU_AVC_PROFILE,	   "AVC Profile"},
    {MUX1_XU_GUID, MUX_XU_AVC_MAX_FRAME_SIZE,32,  0,  V4L2_CID_MUX_CH1_XU_AVC_MAX_FRAME_SIZE,"AVC Max Frame Size"},
    {MUX1_XU_GUID, MUX_XU_AVC_LEVEL,	     32,  0,  V4L2_CID_MUX_CH1_XU_AVC_LEVEL,	   "AVC Level"},
    {MUX1_XU_GUID, MUX_XU_PIC_TIMING_ENABLE, 32,  0,  V4L2_CID_MUX_CH1_XU_PIC_TIMING_ENABLE,"pic timing enable"},
    {MUX1_XU_GUID, MUX_XU_VFLIP,	     32,  0,  V4L2_CID_MUX_CH1_XU_VFLIP,	   "vflip"},
    {MUX1_XU_GUID, MUX_XU_AUDIO_BITRATE,     32,  0,  V4L2_CID_MUX_CH1_XU_AUDIO_BITRATE,   "audio bitrate"},
    
    {MUX1_XU_GUID, MUX_XU_START_CHANNEL,     32,  0,  V4L2_CID_MUX_XU_START_CHANNEL,       "Channel start"},
    {MUX1_XU_GUID, MUX_XU_STOP_CHANNEL,	     32,  0,  V4L2_CID_MUX_XU_STOP_CHANNEL,        "Channel stop"},

    {MUX2_XU_GUID, MUX_XU_RESOLUTION,        32,  0,  V4L2_CID_MUX_CH2_XU_RESOLUTION,      "Resolution"},
    {MUX2_XU_GUID, MUX_XU_FRAMEINTRVL,       32,  0,  V4L2_CID_MUX_CH2_XU_FRAMEINTRVL,     "frame interval"},
    {MUX2_XU_GUID, MUX_XU_COMPRESSION_Q,     32,  0,  V4L2_CID_MUX_CH2_XU_COMPRESSION_Q,   "compression quality"},
    {MUX2_XU_GUID, MUX_XU_PIC_TIMING_ENABLE, 32,  0,  V4L2_CID_MUX_CH2_XU_PIC_TIMING_ENABLE,"pic timing enable"},
    {MUX2_XU_GUID, MUX_XU_GOP_HIERARCHY_LEVEL,32, 0,  V4L2_CID_MUX_CH2_XU_GOP_HIERARCHY_LEVEL, "Gop hierarchy level"},
    {MUX2_XU_GUID, MUX_XU_BITRATE,           32,  0,  V4L2_CID_MUX_CH2_XU_BITRATE,         "Bitrate"},
    {MUX2_XU_GUID, MUX_XU_FORCE_I_FRAME,     32,  0,  V4L2_CID_MUX_CH2_XU_FORCE_I_FRAME,   "Force I Frame"},
    {MUX2_XU_GUID, MUX_XU_VUI_ENABLE,        32,  0,  V4L2_CID_MUX_CH2_XU_VUI_ENABLE,       "vui enable"},
    {MUX2_XU_GUID, MUX_XU_CHTYPE,	     32,  0,  V4L2_CID_MUX_CH2_XU_CHTYPE,     	   "channel type"},
    {MUX2_XU_GUID, MUX_XU_GOP_LENGTH,	     32,  0,  V4L2_CID_MUX_CH2_XU_GOP_LENGTH,      "GOP Length"},
    {MUX2_XU_GUID, MUX_XU_AVC_PROFILE,	     32,  0,  V4L2_CID_MUX_CH2_XU_AVC_PROFILE,	   "AVC Profile"},
    {MUX2_XU_GUID, MUX_XU_AVC_MAX_FRAME_SIZE,32,  0,  V4L2_CID_MUX_CH2_XU_AVC_MAX_FRAME_SIZE,"AVC Max Frame Size"},
    {MUX2_XU_GUID, MUX_XU_AVC_LEVEL,	     32,  0,  V4L2_CID_MUX_CH2_XU_AVC_LEVEL,	   "AVC Level"},

    {MUX3_XU_GUID, MUX_XU_RESOLUTION,        32,  0,  V4L2_CID_MUX_CH3_XU_RESOLUTION,      "Resolution"},
    {MUX3_XU_GUID, MUX_XU_FRAMEINTRVL,       32,  0,  V4L2_CID_MUX_CH3_XU_FRAMEINTRVL,     "frame interval"},
    {MUX3_XU_GUID, MUX_XU_COMPRESSION_Q,     32,  0,  V4L2_CID_MUX_CH3_XU_COMPRESSION_Q,   "compression quality"},
    {MUX3_XU_GUID, MUX_XU_PIC_TIMING_ENABLE, 32,  0,  V4L2_CID_MUX_CH3_XU_PIC_TIMING_ENABLE,"pic timing enable"},
    {MUX3_XU_GUID, MUX_XU_GOP_HIERARCHY_LEVEL,32, 0,  V4L2_CID_MUX_CH3_XU_GOP_HIERARCHY_LEVEL, "Gop hierarchy level"},
    {MUX3_XU_GUID, MUX_XU_BITRATE,           32,  0,  V4L2_CID_MUX_CH3_XU_BITRATE,         "Bitrate"},
    {MUX3_XU_GUID, MUX_XU_FORCE_I_FRAME,     32,  0,  V4L2_CID_MUX_CH3_XU_FORCE_I_FRAME,   "Force I Frame"},
    {MUX3_XU_GUID, MUX_XU_VUI_ENABLE,        32,  0,  V4L2_CID_MUX_CH3_XU_VUI_ENABLE,        "vui enable"},
    {MUX3_XU_GUID, MUX_XU_CHTYPE,	     32,  0,  V4L2_CID_MUX_CH3_XU_CHTYPE,     	   "channel type"},
    {MUX3_XU_GUID, MUX_XU_GOP_LENGTH,	     32,  0,  V4L2_CID_MUX_CH3_XU_GOP_LENGTH,      "GOP Length"},
    {MUX3_XU_GUID, MUX_XU_AVC_PROFILE,	     32,  0,  V4L2_CID_MUX_CH3_XU_AVC_PROFILE,	   "AVC Profile"},
    {MUX3_XU_GUID, MUX_XU_AVC_MAX_FRAME_SIZE,32,  0,  V4L2_CID_MUX_CH3_XU_AVC_MAX_FRAME_SIZE,"AVC Max Frame Size"},
    {MUX3_XU_GUID, MUX_XU_AVC_LEVEL,	     32,  0,  V4L2_CID_MUX_CH3_XU_AVC_LEVEL,	   "AVC Level"},

    {MUX4_XU_GUID, MUX_XU_RESOLUTION,        32,  0,  V4L2_CID_MUX_CH4_XU_RESOLUTION,      "Resolution"},
    {MUX4_XU_GUID, MUX_XU_FRAMEINTRVL,       32,  0,  V4L2_CID_MUX_CH4_XU_FRAMEINTRVL,     "frame interval"},
    {MUX4_XU_GUID, MUX_XU_COMPRESSION_Q,     32,  0,  V4L2_CID_MUX_CH4_XU_COMPRESSION_Q,   "compression quality"},
    {MUX4_XU_GUID, MUX_XU_PIC_TIMING_ENABLE, 32,  0,  V4L2_CID_MUX_CH4_XU_PIC_TIMING_ENABLE,"pic timing enable"},
    {MUX4_XU_GUID, MUX_XU_GOP_HIERARCHY_LEVEL,32, 0,  V4L2_CID_MUX_CH4_XU_GOP_HIERARCHY_LEVEL, "Gop hierarchy level"},
    {MUX4_XU_GUID, MUX_XU_BITRATE,           32,  0,  V4L2_CID_MUX_CH4_XU_BITRATE,         "Bitrate"},
    {MUX4_XU_GUID, MUX_XU_FORCE_I_FRAME,     32,  0,  V4L2_CID_MUX_CH4_XU_FORCE_I_FRAME,   "Force I Frame"},
    {MUX4_XU_GUID, MUX_XU_VUI_ENABLE,        32,  0,  V4L2_CID_MUX_CH4_XU_VUI_ENABLE,       "vui enable"},
    {MUX4_XU_GUID, MUX_XU_CHTYPE,	     32,  0,  V4L2_CID_MUX_CH4_XU_CHTYPE,     	   "channel type"},
    {MUX4_XU_GUID, MUX_XU_GOP_LENGTH,	     32,  0,  V4L2_CID_MUX_CH4_XU_GOP_LENGTH,      "GOP Length"},
    {MUX4_XU_GUID, MUX_XU_AVC_PROFILE,	     32,  0,  V4L2_CID_MUX_CH4_XU_AVC_PROFILE,	   "AVC Profile"},
    {MUX4_XU_GUID, MUX_XU_AVC_MAX_FRAME_SIZE,32,  0,  V4L2_CID_MUX_CH4_XU_AVC_MAX_FRAME_SIZE,"AVC Max Frame Size"},
    {MUX4_XU_GUID, MUX_XU_AVC_LEVEL,	     32,  0,  V4L2_CID_MUX_CH4_XU_AVC_LEVEL,	   "AVC Level"},

    {MUX5_XU_GUID, MUX_XU_RESOLUTION,        32,  0,  V4L2_CID_MUX_CH5_XU_RESOLUTION,      "Resolution"},
    {MUX5_XU_GUID, MUX_XU_FRAMEINTRVL,       32,  0,  V4L2_CID_MUX_CH5_XU_FRAMEINTRVL,     "frame interval"},
    {MUX5_XU_GUID, MUX_XU_COMPRESSION_Q,     32,  0,  V4L2_CID_MUX_CH5_XU_COMPRESSION_Q,   "compression quality"},
    {MUX5_XU_GUID, MUX_XU_PIC_TIMING_ENABLE, 32,  0,  V4L2_CID_MUX_CH5_XU_PIC_TIMING_ENABLE,"pic timing enable"},
    {MUX5_XU_GUID, MUX_XU_GOP_HIERARCHY_LEVEL,32, 0,  V4L2_CID_MUX_CH5_XU_GOP_HIERARCHY_LEVEL, "Gop hierarchy level"},
    {MUX5_XU_GUID, MUX_XU_BITRATE,           32,  0,  V4L2_CID_MUX_CH5_XU_BITRATE,         "Bitrate"},
    {MUX5_XU_GUID, MUX_XU_FORCE_I_FRAME,     32,  0,  V4L2_CID_MUX_CH5_XU_FORCE_I_FRAME,   "Force I Frame"},
    {MUX5_XU_GUID, MUX_XU_VUI_ENABLE,        32,  0,  V4L2_CID_MUX_CH5_XU_VUI_ENABLE,       "vui enable"},
    {MUX5_XU_GUID, MUX_XU_CHTYPE,	     32,  0,  V4L2_CID_MUX_CH5_XU_CHTYPE,     	   "channel type"},
    {MUX5_XU_GUID, MUX_XU_GOP_LENGTH,	     32,  0,  V4L2_CID_MUX_CH5_XU_GOP_LENGTH,      "GOP Length"},
    {MUX5_XU_GUID, MUX_XU_AVC_PROFILE,	     32,  0,  V4L2_CID_MUX_CH5_XU_AVC_PROFILE,	   "AVC Profile"},
    {MUX5_XU_GUID, MUX_XU_AVC_MAX_FRAME_SIZE,32,  0,  V4L2_CID_MUX_CH5_XU_AVC_MAX_FRAME_SIZE,"AVC Max Frame Size"},
    {MUX5_XU_GUID, MUX_XU_AVC_LEVEL,	     32,  0,  V4L2_CID_MUX_CH5_XU_AVC_LEVEL,	   "AVC Level"},

    {MUX6_XU_GUID, MUX_XU_RESOLUTION,        32,  0,  V4L2_CID_MUX_CH6_XU_RESOLUTION,      "Resolution"},
    {MUX6_XU_GUID, MUX_XU_FRAMEINTRVL,       32,  0,  V4L2_CID_MUX_CH6_XU_FRAMEINTRVL,     "frame interval"},
    {MUX6_XU_GUID, MUX_XU_COMPRESSION_Q,     32,  0,  V4L2_CID_MUX_CH6_XU_COMPRESSION_Q,   "compression quality"},
    {MUX6_XU_GUID, MUX_XU_PIC_TIMING_ENABLE, 32,  0,  V4L2_CID_MUX_CH6_XU_PIC_TIMING_ENABLE,"pic timing enable"},
    {MUX6_XU_GUID, MUX_XU_GOP_HIERARCHY_LEVEL,32, 0,  V4L2_CID_MUX_CH6_XU_GOP_HIERARCHY_LEVEL, "Gop hierarchy level"},
    {MUX6_XU_GUID, MUX_XU_BITRATE,           32,  0,  V4L2_CID_MUX_CH6_XU_BITRATE,         "Bitrate"},
    {MUX6_XU_GUID, MUX_XU_FORCE_I_FRAME,     32,  0,  V4L2_CID_MUX_CH6_XU_FORCE_I_FRAME,   "Force I Frame"},
    {MUX6_XU_GUID, MUX_XU_VUI_ENABLE,       32,  0,  V4L2_CID_MUX_CH6_XU_VUI_ENABLE,       "vui enable"},
    {MUX6_XU_GUID, MUX_XU_CHTYPE,	     32,  0,  V4L2_CID_MUX_CH6_XU_CHTYPE,     	   "channel type"},
    {MUX6_XU_GUID, MUX_XU_GOP_LENGTH,	     32,  0,  V4L2_CID_MUX_CH6_XU_GOP_LENGTH,      "GOP Length"},
    {MUX6_XU_GUID, MUX_XU_AVC_LEVEL,	     32,  0,  V4L2_CID_MUX_CH6_XU_AVC_LEVEL,	   "AVC Level"},

    {MUX7_XU_GUID, MUX_XU_RESOLUTION,        32,  0,  V4L2_CID_MUX_CH7_XU_RESOLUTION,      "Resolution"},
    {MUX7_XU_GUID, MUX_XU_FRAMEINTRVL,       32,  0,  V4L2_CID_MUX_CH7_XU_FRAMEINTRVL,     "frame interval"},
    {MUX7_XU_GUID, MUX_XU_COMPRESSION_Q,     32,  0,  V4L2_CID_MUX_CH7_XU_COMPRESSION_Q,   "compression quality"},
    {MUX7_XU_GUID, MUX_XU_PIC_TIMING_ENABLE, 32,  0,  V4L2_CID_MUX_CH7_XU_PIC_TIMING_ENABLE,"pic timing enable"},
    {MUX7_XU_GUID, MUX_XU_GOP_HIERARCHY_LEVEL,32, 0,  V4L2_CID_MUX_CH7_XU_GOP_HIERARCHY_LEVEL, "Gop hierarchy level"},
    {MUX7_XU_GUID, MUX_XU_BITRATE,           32,  0,  V4L2_CID_MUX_CH7_XU_BITRATE,         "Bitrate"},
    {MUX7_XU_GUID, MUX_XU_FORCE_I_FRAME,     32,  0,  V4L2_CID_MUX_CH7_XU_FORCE_I_FRAME,   "Force I Frame"},
    {MUX7_XU_GUID, MUX_XU_VUI_ENABLE,        32,  0,  V4L2_CID_MUX_CH7_XU_VUI_ENABLE,       "vui enable"},
    {MUX7_XU_GUID, MUX_XU_CHTYPE,	     32,  0,  V4L2_CID_MUX_CH7_XU_CHTYPE,     	   "channel type"},
    {MUX7_XU_GUID, MUX_XU_GOP_LENGTH,	     32,  0,  V4L2_CID_MUX_CH7_XU_GOP_LENGTH,      "GOP Length"},
    {MUX7_XU_GUID, MUX_XU_AVC_LEVEL,	     32,  0,  V4L2_CID_MUX_CH7_XU_AVC_LEVEL,	   "AVC Level"},

    {PU_XU_GUID, PU_XU_ANF_ENABLE,	    32,  0,  V4L2_CID_PU_XU_ANF_ENABLE,		"ANF Enable"},
    {PU_XU_GUID, PU_XU_NF_STRENGTH,	    32,  0,  V4L2_CID_PU_XU_NF_STRENGTH,	"NF Strength"},

    {PU_XU_GUID, PU_XU_ADAPTIVE_WDR_ENABLE, 32,  0,  V4L2_CID_PU_XU_ADAPTIVE_WDR_ENABLE,"Adaptive WDR Enable"},
    {PU_XU_GUID, PU_XU_WDR_STRENGTH,        32,  0,  V4L2_CID_PU_XU_WDR_STRENGTH,       "WDR Strength"},
    {PU_XU_GUID, PU_XU_AE_ENABLE,           32,  0,  V4L2_CID_PU_XU_AUTO_EXPOSURE,      "Auto Exposure"},
    {PU_XU_GUID, PU_XU_EXPOSURE_TIME,       32,  0,  V4L2_CID_PU_XU_EXPOSURE_TIME,      "Exposure Time"},
    {PU_XU_GUID, PU_XU_AWB_ENABLE,          32,  0,  V4L2_CID_PU_XU_AUTO_WHITE_BAL,     "Auto White Balance"},
    {PU_XU_GUID, PU_XU_WB_TEMPERATURE,      32,  0,  V4L2_CID_PU_XU_WHITE_BAL_TEMP,     "White Balance Temperature"},
    {PU_XU_GUID, PU_XU_VFLIP,               32,  0,  V4L2_CID_PU_XU_VFLIP,              "Vertical Flip"},
    {PU_XU_GUID, PU_XU_HFLIP,               32,  0,  V4L2_CID_PU_XU_HFLIP,              "Horizontal Flip"},
    {PU_XU_GUID, PU_XU_WB_ZONE_SEL_ENABLE,  32,  0,  V4L2_CID_PU_XU_WB_ZONE_SEL_ENABLE, "White Balance Zone Select"},
    {PU_XU_GUID, PU_XU_WB_ZONE_SEL,         32,  0,  V4L2_CID_PU_XU_WB_ZONE_SEL,        "White Balance Zone"},
    {PU_XU_GUID, PU_XU_EXP_ZONE_SEL_ENABLE, 32,  0,  V4L2_CID_PU_XU_EXP_ZONE_SEL_ENABLE,"Exposure Zone Select"},
    {PU_XU_GUID, PU_XU_EXP_ZONE_SEL,        32,  0,  V4L2_CID_PU_XU_EXP_ZONE_SEL,       "Exposure Zone"},
    {PU_XU_GUID, PU_XU_MAX_ANALOG_GAIN,     32,  0,  V4L2_CID_PU_XU_MAX_ANALOG_GAIN,    "Max Analog Gain"},
    {PU_XU_GUID, PU_XU_HISTO_EQ,            32,  0,  V4L2_CID_PU_XU_HISTO_EQ,           "Histogram Equalization"},
    {PU_XU_GUID, PU_XU_SHARPEN_FILTER,      32,  0,  V4L2_CID_PU_XU_SHARPEN_FILTER,     "Sharpen Filter Level"},
    {PU_XU_GUID, PU_XU_GAIN_MULTIPLIER,     32,  0,  V4L2_CID_PU_XU_GAIN_MULTIPLIER,    "Exposure Gain Multiplier"},
    {PU_XU_GUID, PU_XU_TF_STRENGTH,	    32,  0,  V4L2_CID_PU_XU_TF_STRENGTH,        "Temporal filter strength"},
};

static void map_xu(int fd, struct uvc_xu_data *data)
{
	int ret;
	struct uvc_xu_control_info info;
	struct uvc_xu_control_mapping mapping;

	memcpy(info.entity, data->entity, sizeof(data->entity));
	info.index = data->selector - 1;
	info.selector = data->selector;
	info.size = data->size/8; /* XXX - Assumes we have no composite
				     control mappings */
	info.flags =
		UVC_CTRL_FLAG_SET_CUR |
		UVC_CTRL_FLAG_GET_RANGE ; /* XXX - Should make part of struct */
	ret = ioctl(fd, UVCIOC_CTRL_ADD, &info);
	if(ret && errno != EEXIST)
		WARNING("UVCIOC_CTRL_ADD failed: %s.", strerror(errno));

	mapping.id = data->id;
	memcpy(mapping.name, data->name, sizeof(data->name));
	memcpy(mapping.entity, data->entity, sizeof(data->entity));
	mapping.selector = data->selector;
	mapping.size = data->size;
	mapping.offset = data->offset;
	mapping.v4l2_type = V4L2_CTRL_TYPE_INTEGER;
	mapping.data_type = UVC_CTRL_DATA_TYPE_SIGNED;
	ret = ioctl(fd, UVCIOC_CTRL_MAP_OLD, &mapping);
	if(ret && errno != EEXIST && errno != ENOENT) {
		WARNING("UVCIOC_CTRL_MAP_OLD failed (id = 0x%x): %s\n",
				data->id,
				strerror(errno));
	} else {
		return;
	}
	/* Try newer mapping ioctl, we get here only if MAP_OLD fails */
	ret = ioctl(fd, UVCIOC_CTRL_MAP, &mapping);
	if(ret && errno != EEXIST && errno != ENOENT) {
		WARNING("UVCIOC_CTRL_MAP failed (id = 0x%x): %s\n",
				data->id,
				strerror(errno));
	}
}

static int init_ctrl(video_channel_t ch)
{
	int i;
	struct video_stream *vstream;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to initialize the controls on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	if(ipcam_mode){
		for(i = 0; i < (int)(sizeof(mux_xu_data)/sizeof(mux_xu_data[0])); i++) {
			map_xu(vstream->fd, &mux_xu_data[i]);
		}
	} else {
		for(i = 0; i < (int)(sizeof(skype_xu_data)/sizeof(skype_xu_data[0])); i++) {
			map_xu(vstream->fd, &skype_xu_data[i]);
		}

	for(i = 0; i < (int)(sizeof(mappings)/sizeof(mappings[0])); i++) {
		int ret = ioctl(vstream->fd, UVCIOC_CTRL_MAP_OLD, &mappings[i]);
		if(ret && errno != EEXIST && errno != ENOENT) {
			WARNING("UVCIOC_CTRL_MAP_OLD failed for mapping: id = 0x%x, %s.",
					mappings[i].id,
					strerror(errno));
		}
	}
	}

	return 0;
}

static int init_mmap(video_channel_t ch)
{
	struct v4l2_requestbuffers req;
	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to initialize the memory buffers on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	if (is_mux_stream(vstream) && mux_stream.started) {
		vstream->buffers = mux_stream.buffers;
		vstream->n_buffers = mux_stream.n_buffers;
		return 0;
	}

	CLEAR(req);
	req.count = buffer_count;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(vstream->fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			ERROR(-1, "Memory mapping is not supported.");
		}
		ERROR(-1, "Cannot request v4l2 buffers (VIDIOC_REQBUFS): "
			"%i, %s", errno, strerror(errno));
	}

	CHECK_ERROR(req.count < 2, -1, "Insufficient buffer memory.");

	vstream->buffers = calloc(req.count, sizeof(*vstream->buffers));

	CHECK_ERROR(!vstream->buffers, -1, "Out of memory.");

	for (vstream->n_buffers = 0; vstream->n_buffers < req.count;
			++vstream->n_buffers) {
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = vstream->n_buffers;

		CHECK_ERROR(-1 == xioctl(vstream->fd, VIDIOC_QUERYBUF, &buf), -1,
			"Cannot query v4l2 buffers (VIDIOC_QUERYBUF): "
			"%i, %s", errno, strerror(errno));

		vstream->buffers[vstream->n_buffers].length = buf.length;
		vstream->buffers[vstream->n_buffers].start =
			mmap(NULL /* start anywhere */,
				buf.length,
				PROT_READ | PROT_WRITE /* required */,
				MAP_SHARED /* recommended */,
				vstream->fd, buf.m.offset);

		CHECK_ERROR(
			MAP_FAILED == vstream->buffers[vstream->n_buffers].start,
			-1, "Cannot mmap buffers: %i, %s", errno,
			strerror(errno));

		memset(vstream->buffers[vstream->n_buffers].start, 0xab,
				buf.length);
	}

	if (is_mux_stream(vstream)) {
		mux_stream.buffers = vstream->buffers;
		mux_stream.n_buffers = vstream->n_buffers;
	}

	return 0;
}

static int uninit_mmap(video_channel_t ch)
{
	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to uninitialize the memory buffers for %s channel: "
			"the channel is not enabled.", chan2str(ch));

	if (is_mux_stream(vstream) && mux_stream.ref_count)
		return 0;

	unsigned int i;
	struct v4l2_requestbuffers req;

	for (i = 0; i < vstream->n_buffers; ++i) {
		CHECK_ERROR(-1 == munmap(vstream->buffers[i].start,
					vstream->buffers[i].length),
			-1, "Cannot munmap buffers: %i, %s", errno,
			strerror(errno));
	}

	free(vstream->buffers);

	CLEAR(req);

	req.count = 0;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(vstream->fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			ERROR(-1, "Memory mapping is not supported.");
		}
		ERROR(-1, "Cannot request v4l2 buffers (VIDIOC_REQBUFS): "
			"%i, %s", errno, strerror(errno));
	}
	return 0;
}

/* Get/Set for SECS */
static int get_skype_stream_control(video_channel_t ch, struct StreamFormat *format, int commit)
{

	int ret, i;
	int id = commit ? V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT : V4L2_CID_SKYPE_XU_STREAMFORMATPROBE;
	struct v4l2_ext_controls ctrls;
	struct v4l2_ext_control ctrl[5];
	struct video_stream *vstream;

	vstream = &video_stream[ch];

	/* First set the streamID for the channel we want to get the stream
	 * control from */
	struct v4l2_control control;
	control.id = V4L2_CID_SKYPE_XU_STREAMID;
	control.value = ch; /* video_channel_t and stream id is a 1:1 mapping */
	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1, "Unable to set the stream ID for %s", chan2str(ch));

	/* Get the stream controls */
	memset(&ctrls, 0, sizeof(ctrls));
	ctrls.count = 5;
	ctrls.controls = ctrl;

	memset(ctrl, 0, sizeof(ctrl));
	for(i = 0; i < 5; i++)
		ctrl[i].id = id+i;

	ret = ioctl(vstream->fd, VIDIOC_G_EXT_CTRLS, &ctrls);
	CHECK_ERROR(ret < 0, -1, "VIDIOC_G_EXT_CTRLS failed: %s\n", strerror(errno));

	format->bStreamType = ctrl[0].value;
	format->wWidth = ctrl[1].value;
	format->wHeight = ctrl[2].value;
	format->dwFrameInterval = ctrl[3].value;
	format->dwBitrate = ctrl[4].value;

	return 0;
}

static int set_skype_stream_control(video_channel_t ch, struct StreamFormat *format, int commit)
{
	int ret, i;
	int id = commit ? V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT : V4L2_CID_SKYPE_XU_STREAMFORMATPROBE;
	struct v4l2_ext_controls ctrls;
	struct v4l2_ext_control ctrl[5];
	struct video_stream *vstream;

	vstream = &video_stream[ch];

	/* First set the streamID for the channel we want to set the stream
	 * control to */
	struct v4l2_control control;
	control.id = V4L2_CID_SKYPE_XU_STREAMID;
	control.value = -1;//ch; /* video_channel_t and stream id is a 1:1 mapping */
	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1, "Unable to set the stream ID for %s", chan2str(ch));

	/* Set the stream controls */
	memset(&ctrls, 0, sizeof(ctrls));
	ctrls.count = 5;
	ctrls.controls = ctrl;

	memset(ctrl, 0, sizeof(ctrl));
	for(i = 0; i < 5; i++)
		ctrl[i].id = id+i;
	ctrl[0].value =	format->bStreamType;
	ctrl[1].value =	format->wWidth;
	ctrl[2].value =	format->wHeight;
	ctrl[3].value =	format->dwFrameInterval;
	ctrl[4].value =	format->dwBitrate;

	ret = ioctl(vstream->fd, VIDIOC_S_EXT_CTRLS, &ctrls);
	CHECK_ERROR(ret < 0, -1, "VIDIOC_S_EXT_CTRLS failed: %s\n", strerror(errno));

	return 0;
}


static void event_loop_handle_ipcam(video_channel_t ch, struct v4l2_buffer *buf)
{
	struct video_stream *vstream;
	int ret = 0;

	vstream = &video_stream[ch];

	if (is_mux_stream(vstream)) {
		int channel_id;
		video_info_t info;
		uint8_t *data_buf;
		uint32_t size;
		ret = qbox_parse_header(vstream->buffers[buf->index].start, &channel_id, 
					&info.format, &data_buf, &size, &info.ts);
		if (ret){ 
			TRACE("Wrong mux video format\n");
		} else {
			info.buf_index = buf->index;
			if (((unsigned int)channel_id) <= mux_channel_count) {
				struct video_stream *chstream;
				chstream = &video_stream[channel_id];
				if (chstream->started) {
					chstream->frame_count++;
					chstream->cb(data_buf, size,
						info,
						chstream->cb_user_data);
				} else {
					/* queue the buffer */
					mxuvc_video_cb_buf_done(channel_id, buf->index);
				}
			} else {
				/* handle audio */
			}
		}
		/* skip rest of the mux channels */
		//this has to be done in the main for loop
		//ch = NUM_MUX_VID_CHANNELS;
	} else {
		video_info_t info;
		int channel_id;
		uint8_t *data_buf;
		uint32_t size;
		/* handle luma stream for ipcam */
		if (ipcam_mode && !qbox_parse_header(vstream->buffers[buf->index].start, &channel_id, &info.format, &data_buf, &size, &info.ts)) {
			int w = vstream->fmt.fmt.pix.width;
			int h = vstream->fmt.fmt.pix.height;
			int ssize;
			/* calc size of motion stats */
			w = ((w + 15) / 16);
			h = ((h + 15) / 16);
			w = (w + 7) & ~0x7;
			ssize = w * h * 16;
            ssize = (ssize + 255) & ~0xFF;
			info.stats.size = ssize;
			info.stats.buf = data_buf;
			info.buf_index = buf->index;
			data_buf = (uint8_t*)((uint32_t)data_buf +
						ssize);
			size -= ssize;
			vstream->frame_count++;
			vstream->cb(data_buf, size, info,
				vstream->cb_user_data);
		} else {
			struct timeval tv;
			uint64_t ts64 = 0, tsec = 0, tusec = 0;
			gettimeofday(&tv, NULL);
			tsec = (uint64_t) tv.tv_sec;
			tusec = (uint64_t) tv.tv_usec;
			ts64 = (tsec*1000000 + tusec)*9/100;
			info.ts = (uint32_t) (ts64 & 0xffffffff);

			info.format = vstream->cur_vfmt;
			info.stats.buf = NULL;
			info.stats.size = 0;
			info.buf_index = buf->index;
			vstream->frame_count++;
			vstream->cb(vstream->buffers[buf->index].start,
				buf->bytesused, info,
				vstream->cb_user_data);
		}
	}
}

static void event_loop_handle_secs(video_channel_t ch, struct v4l2_buffer *buf)
{
	struct video_stream *vstream;
	video_info_t info;
	frame_t *frame;
	payload_t *payload;
	parse_error_t err = PARSE_NO_ERROR;

	vstream = &video_stream[ch];

	/* Decompose the packet */
	err = SkypeECXU_ValidateFrame(vstream->buffers[buf->index].start,
			buf->bytesused, &frame);
	CHECK_ERROR(err != PARSE_NO_ERROR, , "%s", SkypeECXU_ParserError(err));

	/* Extract the payload */
	err = SkypeECXU_ExtractPayload(ch, frame, &payload);
	CHECK_ERROR(err != PARSE_NO_ERROR, , "%s", SkypeECXU_ParserError(err));

	/* Double check the video format and update if necessary */
	if(secs_format_mapping[vstream->cur_vfmt] != (int) payload->s_type) {
		unsigned int i;
		for(i=0; i<sizeof(secs_format_mapping); ++i)
			if(secs_format_mapping[i] == (int) payload->s_type) {
				vstream->cur_vfmt = i;
			}
	}
	/* Fill info and run the callback */
	info.ts = payload->PTS;
	info.format = vstream->cur_vfmt;
	info.stats.buf = NULL;
	info.stats.size = 0;
	info.buf_index = buf->index;
	vstream->frame_count++;
	vstream->cb(payload->payload, payload->len, info, vstream->cb_user_data);
}

static void event_loop_handle_default(video_channel_t ch, struct v4l2_buffer *buf)
{
	struct video_stream *vstream;
	video_info_t info;
	struct timeval tv;
	uint64_t ts64 = 0, tsec = 0, tusec = 0;

	vstream = &video_stream[ch];

	/* Check if the camera has entered SECS mode. It is possible that the
	 * camera was used in SECS mode before mxuvc was started. In that case
	 * the only way to know is to check the header of the stream packets.
	 * We only do this check for the first video frame */
	if(vstream->frame_count == 0 && vstream->secs_supported) {
		frame_t *frame;
		parse_error_t err = PARSE_NO_ERROR;
		err = SkypeECXU_ValidateFrame(vstream->buffers[buf->index].start,
				buf->bytesused, &frame);
		if(err == PARSE_NO_ERROR) {
			vstream->secs_enabled = 1;
			event_loop_handle_secs(ch, buf);
			return;
		}
	}

	/* Generate a timestamp */
	gettimeofday(&tv, NULL);
	tsec = (uint64_t) tv.tv_sec;
	tusec = (uint64_t) tv.tv_usec;
	ts64 = (tsec*1000000 + tusec)*9/100;

	/* Fill info and run the callback */
	info.ts = (uint32_t) (ts64 & 0xffffffff);
	info.format = vstream->cur_vfmt;
	info.stats.buf = NULL;
	info.stats.size = 0;
	info.buf_index = buf->index;
	vstream->frame_count++;
	vstream->cb(vstream->buffers[buf->index].start,
			buf->bytesused, info,
			vstream->cb_user_data);
}

static int exit_event_loop = 0;
static int event_loop_started = 0;
static void *event_loop(void *ptr)
{
	int ch, r, max_fd;
	fd_set fds;
	static int sequence[NUM_VID_CHANNEL] = {[0 ... (NUM_VID_CHANNEL-1)] = -1};
	struct video_stream *vstream;
	struct timeval timeout;
	int channel_cnt;

	if(ipcam_mode)
		channel_cnt = NUM_IPCAM_VID_CHANNELS;
	else
		channel_cnt = NUM_VID_CHANNEL;

	event_loop_started = 1;

	TRACE("Entering camera event loop\n");
	while(!exit_event_loop) {
		/* A 5 second timeout is used for select on fd */
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		max_fd=0;
		FD_ZERO(&fds);
		mux_stream.waiting = 0;
		for (ch=0; ch<channel_cnt; ch++) {
			vstream = &video_stream[ch];
			/* Build the fd set to watch */
			if (vstream->started == 1) {
				if (is_mux_stream(vstream) &&
					!mux_stream.waiting) {
					mux_stream.waiting = 1;
					FD_SET(vstream->fd, &fds);
				} else
					FD_SET(vstream->fd, &fds);
				if (vstream->fd > max_fd)
					max_fd = vstream->fd;
			}
		}
		if(max_fd == 0) {
			exit_event_loop = 1;
			break;
		}

		r = select(max_fd + 1, &fds, NULL, NULL, &timeout);
		if (-1 == r) {
			if (EINTR == errno)
				continue;
			ERROR_NORET("select: %i, %s. Exiting", errno,
					strerror(errno));
			break;
		}
		if (r == 0)
			continue;

		/*********************************
		 *    Read the received frame    *
		 *********************************/
		for (ch=0; ch<channel_cnt; ch++) {
			vstream = &video_stream[ch];
			if ((vstream->started != 1) ||
				FD_ISSET(video_stream[ch].fd, &fds) == 0)
				continue;

			int fd = vstream->fd;
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;

			if (!is_mux_stream(vstream) && vstream->started == 0) {
				TRACE("RACE condition before DQBUF. Skipping.");
				continue;
			}

			if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
				switch (errno) {
				case EAGAIN:
					ERROR_NORET("VIDIOC_DQBUF returned EAGAIN.");
					break;

				case EIO:
					/* Could ignore EIO, see spec. */
					ERROR_NORET("VIDIOC_DQBUF returned EIO.");
					break;

				default:
					perror("VIDIOC_DQBUF");
					break;
				}
				/* Error: we stop the channel */
				mxuvc_video_stop(ch);
				continue;
			}

			if (!is_mux_stream(vstream) &&
				(buf.sequence && buf.sequence !=
					(unsigned int)(sequence[ch] + 1))) {
				WARNING("sequence mismatch expected %d got %d "
					"- frames were missed, expect errors "
					"or encoder/decoder drift\n",
					sequence[ch]+1, buf.sequence);
			}
			sequence[ch] = buf.sequence;

			assert(buf.index < vstream->n_buffers);

			/*************************
			 *    Handle the data    *
			 *************************/
			/* Mux streams */
			if (ipcam_mode){
				event_loop_handle_ipcam(ch, &buf);
				
				/* skip rest of the mux channels */
				if (is_mux_stream(vstream))
					ch = mux_channel_count;
			/* Handle SECS */
			}else if (vstream->secs_enabled)
				event_loop_handle_secs(ch, &buf);
			/* Handle non SECS */
			else
				event_loop_handle_default(ch, &buf);

			/**********************
			 *    Final checks    *
			 **********************/
			if ((is_mux_stream(vstream) &&
				mux_stream.started == 0) ||
				(!is_mux_stream(vstream) &&
					vstream->started == 0)) {
				TRACE("RACE condition before QBUF. Skipping.");
				continue;
			}

			if (!ipcam_mode && (-1 == xioctl(fd, VIDIOC_QBUF, &buf))) {
				perror("VIDIOC_QBUF");
				mxuvc_video_stop(ch);
				continue;
			}
		}
	}
	TRACE("Exiting camera event loop\n");
	event_loop_started = 0;
	return 0;
}

int mxuvc_video_cb_buf_done(video_channel_t ch, int buf_index)
{
	struct v4l2_buffer buf;
	struct video_stream *vstream = &video_stream[ch];

	/* API is valid only for ipcam mode */
	if (!ipcam_mode)
		return 0;

	/* do not queue the buffer if stream is stopped */
	if ((is_mux_stream(vstream) && !mux_stream.started) ||
	    (!is_mux_stream(vstream) && !vstream->started))
		return 0;

	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = buf_index;
	if (-1 == xioctl(vstream->fd, VIDIOC_QBUF, &buf)) {
		perror("VIDIOC_QBUF");
		mxuvc_video_stop(ch);
		return -1;
	}

	return 0;
}

int channel_init(video_channel_t ch, char *dev_name)
{
	int ret;
	struct video_stream *vstream;
	vstream = &video_stream[ch];

	TRACE("Initializing %s channel using %s\n", chan2str(ch), dev_name);

	/* Open device */
	struct stat st;

	CHECK_ERROR(-1 == stat(dev_name, &st), -1,
			"Cannot identify '%s': %d, %s.", dev_name, errno,
			strerror(errno));

	CHECK_ERROR(!S_ISCHR(st.st_mode), -1, "%s is no device.", dev_name);

	if (ipcam_mode && (ch < mux_channel_count))
		vstream->fd = mux_stream.fd;
	else
		vstream->fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

	CHECK_ERROR(-1 == vstream->fd, -1, "Cannot open '%s': %d, %s.",
			dev_name, errno, strerror(errno));

	vstream->enabled = 1;
	vstream->started = 0;

	/* Init device */
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;

	if (-1 == xioctl(vstream->fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			ERROR(-1, "%s is no V4L2 device.", dev_name);
		} else {
			perror("VIDIOC_QUERYCAP");
			return -1;
		}
	}

	CHECK_ERROR(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE), -1,
			"%s is no video capture device.", dev_name);

	CHECK_ERROR(!(cap.capabilities & V4L2_CAP_STREAMING), -1,
			"%s does not support streaming i/o.", dev_name);

	/* Select video input, video standard and tune here. */
	CLEAR(cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (0 == xioctl(vstream->fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(vstream->fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
				case EINVAL:
					/* Cropping not supported. */
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	} else {
		/* Errors ignored. */
	}

	/* Get/Use default video settings */
	CLEAR(vstream->fmt);
	vstream->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	CHECK_ERROR(-1 == xioctl(vstream->fd, VIDIOC_G_FMT, &vstream->fmt), -1,
		"Cannot get default video settings: %i, %s",
		errno, strerror(errno));
	ret = fourcc2vidformat(vstream->fmt.fmt.pix.pixelformat, &vstream->cur_vfmt);
	CHECK_ERROR(ret < 0, -1, "Unable to determine the default video format.");

	/* Buggy driver paranoia. */
	unsigned int min;
	min = vstream->fmt.fmt.pix.width * 2;
	if (vstream->fmt.fmt.pix.bytesperline < min)
		vstream->fmt.fmt.pix.bytesperline = min;
	min = vstream->fmt.fmt.pix.bytesperline * vstream->fmt.fmt.pix.height;
	if (vstream->fmt.fmt.pix.sizeimage < min)
		vstream->fmt.fmt.pix.sizeimage = min;

	/* Map controls */
	ret = init_ctrl(ch);
	if(ret < 0)
		return -1;

	if(!ipcam_mode){
		/* Check whether the channel supports SECS by querying the Version control
		 *  from the Skype XU */
		struct v4l2_control control;
		control.id = V4L2_CID_SKYPE_XU_VERSION;
		ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
		if(ret>=0) {
			TRACE("SECS support detected on %s\n", chan2str(ch));
			vstream->secs_supported = 1;
		}
	}else
		vstream->secs_supported = 0;

	/* Get and cache the initial video settings */
	vstream->cur_width = 0; vstream->cur_height = 0;
	vstream->cur_framerate = 0;
	vstream->cur_bitrate = 0;
	mxuvc_video_get_resolution(ch, &(vstream->cur_width), &(vstream->cur_height));
	mxuvc_video_get_framerate(ch, &(vstream->cur_framerate));
	switch(vstream->cur_vfmt) {
		case VID_FORMAT_H264_RAW:
		case VID_FORMAT_H264_TS:
		case VID_FORMAT_H264_AAC_TS:
			mxuvc_video_get_bitrate(ch, &(vstream->cur_bitrate));
			break;
		default:
			break;
	}

	/* Event thread/loop */
	exit_event_loop = 0;

	return 0;
}

static int get_camera_type(char *dev_name, int *cam_mode)
{
	int fd;
	struct v4l2_format fmt;
	uint32_t fourcc;

	/* Open device */
	struct stat st;

	CHECK_ERROR(-1 == stat(dev_name, &st), -1,
			"Cannot identify '%s': %d, %s.", dev_name, errno,
			strerror(errno));

	CHECK_ERROR(!S_ISCHR(st.st_mode), -1, "%s is no device.", dev_name);

	fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);

	CHECK_ERROR(-1 == fd, -1, "Cannot open '%s': %d, %s.",
			dev_name, errno, strerror(errno));

	/* Init device */
	struct v4l2_capability cap;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			ERROR(-1, "%s is no V4L2 device.", dev_name);
		} else {
			perror("VIDIOC_QUERYCAP");
			return -1;
		}
	}

	CHECK_ERROR(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE), -1,
			"%s is no video capture device.", dev_name);

	CHECK_ERROR(!(cap.capabilities & V4L2_CAP_STREAMING), -1,
			"%s does not support streaming i/o.", dev_name);

	/* Get/Use default video settings */
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	CHECK_ERROR(-1 == xioctl(fd, VIDIOC_G_FMT, &fmt), -1,
		"Cannot get default video settings: %i, %s",
		errno, strerror(errno));

	CHECK_ERROR(-1 == vidformat2fourcc(VID_FORMAT_MUX, &fourcc), -1,
		"Unable to determine fourcc code");
	/* check whether ip camera mode */
	/* TODO make an enum or macro of camera modes */
	if (fmt.fmt.pix.pixelformat == fourcc)
		*cam_mode = 1;
	else
		*cam_mode = 0;

	close(fd);

	return 0;
}

int mxuvc_video_init(const char *backend, const char *options)
{
	RECORD("\"%s\", \"%s\"", backend, options);

	int ret, dev_offset, dev_offset_secondary = 0;
	char *str = NULL, *opt, *value;
	char dev_name[16] = "/dev/videoX";
	uint32_t channel;
	uint32_t ch_count = 0;

	TRACE("Initializing the video\n");

	/* Check that the correct video backend was requested*/
	if(strncmp(backend, "v4l2", 4)) {
		ERROR(-1, "The video backend requested (%s) does not match "
			"the implemented one (v4l2)", backend);
 	}

	/* Set init parameters to their default values */
	dev_offset = DEV_OFFSET_DEFAULT;
	dev_offset_secondary = 0;

	/* Get backend option from the option string */
	if (options != NULL) {
		str = malloc(strlen(options)+1);
		CHECK_ERROR(str == NULL, -1,
				"Unable to initialize video: out of memory");
		strcpy(str, options);
	}
	ret = next_opt(str, &opt, &value);
	while (ret == 0) {
		if (strcmp(opt, "dev_offset") == 0)
			dev_offset = (int) strtoul(value, NULL, 10);
		else if (strcmp(opt, "dev_offset_secondary") == 0) {
			dev_offset_secondary = (int) strtoul(value, NULL, 10);
		} else if (strcmp(opt, "v4l_buffers") == 0) {
			buffer_count = (int) strtoul(value, NULL, 10);
		} else
			WARNING("Unrecognized option: '%s'", opt);
		ret = next_opt(NULL, &opt, &value);
	}

	/* if secondary device is not specified, use dev_offset+1 */
	if (!dev_offset_secondary)
		dev_offset_secondary = dev_offset + 1;

	/* Display the values we are going to use */
	TRACE("Using dev_offset = %i\n", dev_offset);
	TRACE("Using dev_offset_secondary = %i\n", dev_offset_secondary);
	TRACE("Using v4l_buffers = %i\n", buffer_count);

	/* Initialize the video channels */
	CHECK_ERROR(dev_offset >= 9, -1, "Unable to initialize the video: "
			"dev_offset must be less than 9.");

	sprintf(dev_name, "/dev/video%d", dev_offset);

	/* check camera mode
	 * open first device (/dev/video<dev_offset>) and check for mux format
	 * if mux format is present, switch to ipcam mode, otherwise continue in
	 * skype mode
	 */
	ret = get_camera_type(dev_name, &ipcam_mode);
	CHECK_ERROR(ret < 0, -1, "Unable to initialize camera");

	if (ipcam_mode == 0) {
		/* skype mode; continue normal init */
		/* allocate memory for skype stream information */
		video_stream = (struct video_stream*)malloc(sizeof(struct video_stream) * NUM_SKYPE_VID_CHANNELS);
		CHECK_ERROR(video_stream == NULL, -1, "Failed to allocate memory");
		memset((void *)video_stream, 0, sizeof(struct video_stream)*NUM_SKYPE_VID_CHANNELS);

		ret = channel_init(CH_MAIN, dev_name);
		CHECK_ERROR(ret < 0, -1, "Unable to initialize video main channel.");
		sprintf(dev_name, "/dev/video%d", dev_offset_secondary);
		ret = channel_init(CH_PREVIEW, dev_name);
		CHECK_ERROR(ret < 0, -1, "Unable to initialize video preview channel.");
	} else {
		/* camera in ipcam mode */
		video_stream = (struct video_stream*)malloc(sizeof(struct video_stream) * NUM_IPCAM_VID_CHANNELS);
		CHECK_ERROR(video_stream == NULL, -1, "Failed to allocate memory");
		memset((void *)video_stream, 0, sizeof(struct video_stream)* NUM_IPCAM_VID_CHANNELS);

		mux_stream.ref_count = 0;
		mux_stream.fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
		CHECK_ERROR(-1 == mux_stream.fd, -1, "Cannot open '%s': %d, %s.",
				dev_name, errno, strerror(errno));

		ret = channel_init(CH1, dev_name);
		CHECK_ERROR(ret < 0, -1, "Unable to initialize local video channel.");
		ret = mxuvc_video_get_channel_count(&ch_count);

		mux_channel_count = ch_count - 1; //drop the Raw channel from the mux channel count
		CHECK_ERROR(mux_channel_count > NUM_MUX_VID_CHANNELS, -1, "Invalid Mux channel count received");

		for(channel = CH2 ; channel < ch_count - 1; channel++)
		{
			ret = channel_init(channel, dev_name);
			CHECK_ERROR(ret < 0, -1, "Unable to initialize local video channel.");
		}
		sprintf(dev_name, "/dev/video%d", dev_offset_secondary);
		ret = channel_init(channel, dev_name);
		CHECK_ERROR(ret < 0, -1, "Unable to initialize raw video channel.");
	}

	return 0;
}


int channel_deinit(video_channel_t ch)
{
	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
		"Cannot deinitialize %s channel: the channel is not enabled.",
		chan2str(ch));

	exit_event_loop = 1;
	if (vstream->started)
		mxuvc_video_stop(ch);
	vstream->enabled = 0;
	if (!ipcam_mode ||
	   (ipcam_mode && !is_mux_stream(vstream)) ||
	   (ipcam_mode && is_mux_stream(vstream) && mux_stream.ref_count == 0)){
		close(vstream->fd);
	}

	return 0;
}

int mxuvc_video_deinit()
{
	uint32_t channel; 
	int ret;
	uint32_t ch_count = 0;
	RECORD("");

	if (ipcam_mode) {
		ret = mxuvc_video_get_channel_count(&ch_count);
		for(channel = CH1 ; channel < ch_count ; channel++){
			channel_deinit(channel);
		}
	} else {
		channel_deinit(CH_MAIN);
		channel_deinit(CH_PREVIEW);
	}

	TRACE("The video has been successfully uninitialized\n");
	return 0;
}

int mxuvc_video_alive()
{
	RECORD("");
	return event_loop_started;
}

#if 0
static int get_ctrl(video_channel_t ch, int id, int *value)
{
	struct v4l2_control control;
	int ret;

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
		"Cannot get the control %i on %s channel: the channel "
		"is not enabled.", id, chan2str(ch));

	control.id = id;
	TRACE2("Getting value for control 0x%x.\n", control.id);

	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to get the control value (id = 0x%x): %s.",
		id, strerror(errno));

	TRACE2("Control 0x%x = %d\n", control.id, control.value);
	*value = control.value;
	return 0;
}

static int set_ctrl(video_channel_t ch, int id, int value)
{
	struct v4l2_control control;
	int ret;

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
		"Cannot set the control %i on %s channel: the channel "
		"is not enabled.", id, chan2str(ch));

	control.id = id;
	control.value = value;
	TRACE2("Setting control 0x%x to %d.\n", control.id, control.value);

	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to set control 0x%x to the requested value (%i): %s.",
		id, value, strerror(errno));

	return 0;
}
#endif
int mxuvc_video_start(video_channel_t ch)
{
	RECORD("%s", chan2str(ch));
	int i, ret;
	enum v4l2_buf_type type;
	struct v4l2_buffer buf;
	static pthread_t event_thread;

	TRACE("Starting the video on %s channel.\n", chan2str(ch));

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to start the video on %s channel: "
			"the channel is not enabled.", chan2str(ch));
	CHECK_ERROR(vstream->started == 1, -1,
			"Unable to start the video on %s channel: "
			"the video is already started.", chan2str(ch));

	TRACE("Starting video.\n");

	/* Allocate/map memory for streaming */
	ret = init_mmap(ch);
	if(ret < 0)
		return -1;
	//start xu
	if (is_mux_stream(vstream)){
		uint32_t data = ch;
		getset_mux_channel_param(CH1, (void *)&data, sizeof(uint32_t),
						MUX_XU_START_CHANNEL, 1);
	}

	if (!is_mux_stream(vstream) ||
	    (is_mux_stream(vstream) && !mux_stream.started)) {
		for (i = 0; i < (int)vstream->n_buffers; ++i) {
			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;
			CHECK_ERROR(-1 == xioctl(vstream->fd, VIDIOC_QBUF, &buf), -1,
				"Cannot queue buffers (VIDIOC_QBUF): %i, %s",
				errno, strerror(errno));
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		CHECK_ERROR(-1 == xioctl(vstream->fd, VIDIOC_STREAMON, &type), -1,
			"Cannot start streaming (VIDIOC_STREAMON): %i, %s",
			errno, strerror(errno));

		vstream->frame_count = 0;
		vstream->started = 1;
		/* start event loop */
		if (event_loop_started == 0) {
			exit_event_loop = 0;
			ret = pthread_create(&event_thread, NULL, &event_loop,
						NULL);
			if (ret) {
				ERROR_NORET("Failed to start event loop: "
						"%i, %s", ret, strerror(ret));
				mxuvc_video_stop(ch);
				return ret;
			}
		}
		/* wait till event loop starts */
		while (event_loop_started == 0)
			usleep(50);
		if (is_mux_stream(vstream)) {
			mux_stream.started = 1;
			mux_stream.ref_count++;
		}
	} else if (is_mux_stream(vstream) && mux_stream.started) {
		vstream->frame_count = 0;
		vstream->started = 1;
		mux_stream.ref_count++;
	}

	return 0;
}

int mxuvc_video_stop(video_channel_t ch)
{
	RECORD("%s", chan2str(ch));
	TRACE("Stopping the video on %s channel.\n", chan2str(ch));

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to stop the video on %s channel: "
			"the channel is not enabled.", chan2str(ch));
	CHECK_ERROR(vstream->started == 0, -1,
			"Unable to stop the video on %s channel: "
			"the video has not been started.", chan2str(ch));

	TRACE("%s: %u video frames captured.\n", chan2str(ch),
			vstream->frame_count);

	if (is_mux_stream(vstream)) {
		//stop xu
		uint32_t data = ch;
		getset_mux_channel_param(CH1, (void *)&data, sizeof(uint32_t),
						MUX_XU_STOP_CHANNEL, 1);

		mux_stream.ref_count--;
		vstream->started = 0;
		if (mux_stream.ref_count)
			return 0;
	}

	int ret;
	enum v4l2_buf_type type;

	TRACE("Stopping video.\n");
	vstream->started = 0;
	if (is_mux_stream(vstream))
		mux_stream.started = 0;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(vstream->fd, VIDIOC_STREAMOFF, &type)) {
		WARNING("VIDIOC_STREAMOFF: %i, %s",
					errno, strerror(errno));
	}
	ret = uninit_mmap(ch);
	CHECK_ERROR(ret < 0, -1, "Cannot free the mapped memory.");

	return 0;
}

int mxuvc_video_register_cb(video_channel_t ch, mxuvc_video_cb_t func,
		void *user_data)
{
	RECORD("%s, %p, %p", chan2str(ch), func, user_data);
	TRACE("Registering callback function for %s channel.\n", chan2str(ch));

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to register the callback function for %s "
			"channel: the channel is not enabled.", chan2str(ch));

	vstream->cb = func;
	vstream->cb_user_data = user_data;

	return 0;

}

int mxuvc_video_set_format(video_channel_t ch, video_format_t fmt)
{
	RECORD("%s, %s", chan2str(ch), vidformat2str(fmt));
	TRACE("Setting the video format to %s on %s channel.\n",
			vidformat2str(fmt), chan2str(ch));

	struct video_stream *vstream;
	int ret;
	int restart = 0;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set the video format for %s "
			"channel. The channel is not enabled.", chan2str(ch));

	CHECK_ERROR(fmt >= NUM_VID_FORMAT, -1, "An unknown video format was "
			"requested for %s\n", chan2str(ch));

	/* Return if the current format correspond to the format requested */
	if(fmt == vstream->cur_vfmt)
		return 0;

	/* SECS method: only if the SECS XU support the video format requested */
	if(vstream->secs_supported && secs_format_mapping[fmt] >= 0) {
		/* Make sure we are in UVC standard MJPEG with a resolution of at least 720p */
		if(vstream->fmt.fmt.pix.pixelformat != v4l2_fourcc('M','J','P','G')
				|| vstream->fmt.fmt.pix.width < 1280
				|| vstream->fmt.fmt.pix.height < 720) {

			/* Set the format to MJPEG */
			vstream->fmt.fmt.pix.pixelformat = v4l2_fourcc('M','J','P','G');

			/* Set the resolution to 720p */
			vstream->fmt.fmt.pix.width = 1280;
			vstream->fmt.fmt.pix.height = 720;

			/* Stop video streaming if necessary before changing format */
			if(vstream->started) {
				ret = mxuvc_video_stop(ch);
				CHECK_ERROR(ret < 0, -1, "Failed to set the video format: "
						"could not stop video streaming on %s "
						"channel.", chan2str(ch));
				restart = 1;
			}

			/* Commit the change */
			ret = xioctl(vstream->fd, VIDIOC_S_FMT, &vstream->fmt);
		}

		/* Enable SECS */
		struct StreamFormat format;

		/* Fill with the current values */
		format.wWidth = vstream->cur_width;
		format.wHeight = vstream->cur_height;
		format.dwFrameInterval = FRI(vstream->cur_framerate);
		format.dwBitrate = vstream->cur_bitrate;

		/* Set the stream type to the requested value */
		format.bStreamType = secs_format_mapping[fmt];

		/* Trying setting the format with a SET PROBE */
		ret = set_skype_stream_control(ch, &format, 0);
		if(ret < 0) {
			ERROR_NORET("Unable to set/probe the video format to "
				"%s on %s", vidformat2str(fmt), chan2str(ch));
			if(restart) mxuvc_video_start(ch);
			return -1;
		}

		/* Check if the camera accepted the requested format */
		get_skype_stream_control(ch, &format, 0);
		if(format.bStreamType != secs_format_mapping[fmt]) {
			ERROR_NORET("The camera does not support %s on %s",
				vidformat2str(fmt), chan2str(ch));
			if(restart) mxuvc_video_start(ch);
			return -1;
		}

		/* Everything is OK, COMMIT the format */
		/* Stop video streaming if necessary before changing format */
		if(vstream->started) {
			ret = mxuvc_video_stop(ch);
			CHECK_ERROR(ret < 0, -1, "Failed to set the video format: "
					"could not stop video streaming on %s "
					"channel.", chan2str(ch));
			restart = 1;
		}

		ret = set_skype_stream_control(ch, &format, 1);
		if(ret < 0) {
			ERROR_NORET("Unable to commit the video format to "
				"%s on %s", vidformat2str(fmt), chan2str(ch));
			if(restart) mxuvc_video_start(ch);
			return -1;
		}

		vstream->secs_enabled = 1;

		/* Update the cached stream format settings */
		vstream->cur_width = format.wWidth;
		vstream->cur_height = format.wHeight;
		vstream->cur_framerate = FRI(format.dwFrameInterval);
		vstream->cur_bitrate = format.dwBitrate;
	}
	/* Non SECS method */
	else {
		if((vstream->cur_vfmt == VID_FORMAT_MUX) && (ipcam_mode)){
			errno = EPERM;
			ERROR_NORET("Unable to set the video format on %s channel: %s"
					,chan2str(ch), strerror(errno));
			return -1;
		}
		ret = vidformat2fourcc(fmt, &vstream->fmt.fmt.pix.pixelformat);
		if(ret < 0) {
			ERROR_NORET("No pixelformat corresponds to %s "
				"video format", vidformat2str(fmt));
			if(restart) mxuvc_video_start(ch);
			return -1;
		}

		/* Stop video streaming if necessary before changing format */
		if(vstream->started) {
			ret = mxuvc_video_stop(ch);
			CHECK_ERROR(ret < 0, -1, "Failed to set the video format: "
					"could not stop video streaming on %s "
					"channel.", chan2str(ch));
			restart = 1;
		}

		ret = xioctl(vstream->fd, VIDIOC_S_FMT, &vstream->fmt);
		if(ret == -1) {
			ERROR_NORET("Unable to set the video format on %s channel: "
				"VIDIOC_S_FMT failed: %s", chan2str(ch), strerror(errno));
			if(restart) mxuvc_video_start(ch);
			return -1;
		}
	}

	vstream->cur_vfmt = fmt;

	/* Enable AAC muxing if necessary */
	if((vstream->fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MPEG) &&
		(((fmt == VID_FORMAT_H264_AAC_TS) && (vstream->mux_aac_ts == 0)) ||
		((fmt != VID_FORMAT_H264_AAC_TS) && (vstream->mux_aac_ts != 0)))) {

		struct v4l2_control control;
		control.id = V4L2_CID_XU_AV_MUX_ENABLE;

		if (fmt == VID_FORMAT_H264_AAC_TS)
			control.value = 1;
		else
			control.value = 0;

		ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
		if(ret < 0) {
			ERROR_NORET("Unable to %s AAC muxing on %s channel: "
				"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
				control.value == 1 ? "enable" : "disable",
				chan2str(ch), control.id, control.value,
				strerror(errno));
			if(restart) mxuvc_video_start(ch);
			return -1;
		}
		vstream->mux_aac_ts = control.value;
	}

	/* Restart video streaming if necessary */
	if(restart) {
		ret = mxuvc_video_start(ch);
		CHECK_ERROR(ret < 0, -1, "Failed to set the video format: "
				"could not restart video streaming on %s "
				"channel.", chan2str(ch));
	}

	return 0;
}

int mxuvc_video_get_format(video_channel_t ch, video_format_t *fmt)
{
	RECORD("%s, %p", chan2str(ch), fmt);
	TRACE2("Getting the video format on %s channel.\n", chan2str(ch));

	struct video_stream *vstream;
	uint32_t fourcc = 0;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get the video format for %s "
			"channel. The channel is not enabled.", chan2str(ch));
	if((vstream->cur_vfmt == VID_FORMAT_MUX) && (ipcam_mode)){
		getset_mux_channel_param(ch, (void *)&fourcc, sizeof(uint32_t),
							MUX_XU_CHTYPE, 0);
		video_format_t format;
		fourcc2vidformat(fourcc, &format);
		*fmt = format;
	} else
		*fmt = vstream->cur_vfmt;

	return 0;
}

int mxuvc_video_set_bitrate (video_channel_t ch, uint32_t value)
{
	RECORD("%s, %i", chan2str(ch), value);
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret, id;

	TRACE("Setting the bitrate to %d on %s channel\n", value,
			chan2str(ch));

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set the bitrate on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	if((vstream->cur_vfmt == VID_FORMAT_MUX) && ipcam_mode)
	{
		id = MUX_XU_BITRATE;
		getset_mux_channel_param(ch,
				(void *)&value,
				sizeof(uint32_t),
				id,
				1);
	} else {
		if(vstream->secs_enabled)
			control.id = V4L2_CID_SKYPE_XU_BITRATE;
		else
			control.id = V4L2_CID_XU_BITRATE;
		control.value = value;
		ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
		CHECK_ERROR(ret < 0, -1,
			"Unable to set the bitrate to %i on %s channel: "
			"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
			control.value, chan2str(ch), control.id, control.value,
			strerror(errno));
	}

	vstream->cur_bitrate = value;

	return 0;
}

int mxuvc_video_get_bitrate(video_channel_t ch, uint32_t *value)
{
	RECORD("%s, %p", chan2str(ch), value);
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret, id;

	TRACE("Getting the bitrate on %s channel\n", chan2str(ch));

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get the bitrate on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	if((vstream->cur_vfmt == VID_FORMAT_MUX) && ipcam_mode)
	{
		id = MUX_XU_BITRATE;
		getset_mux_channel_param(ch,
				(void *)value,
				sizeof(uint32_t),
				id,
				0);
	} else {
		if(vstream->secs_enabled)
			control.id = V4L2_CID_SKYPE_XU_BITRATE;
		else
			control.id = V4L2_CID_XU_BITRATE;
		ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
		CHECK_ERROR(ret < 0, -1, "Unable to get the bitrate on %s "
				"channel: VIDIOC_G_CTRL failed (id = 0x%x): %s",
				chan2str(ch), control.id, strerror(errno));

		TRACE2("bitrate = %i\n", control.value);
		*value = control.value;
	}

	vstream->cur_bitrate = *value;

	return 0;
}


int mxuvc_video_get_channel_count(uint32_t *count)
{
	RECORD("%p", count);

	if(ipcam_mode){
		int data = 0;
		getset_mux_channel_param(CH1, (void *)&data, sizeof(uint32_t),
							MUX_XU_CHCOUNT, 0);
		/* Add RAW channel also in channel count */
		*count = data + 1;
	} else
		*count = 2; /* Skype */

	return 0;
}

int mxuvc_video_set_resolution(video_channel_t ch, uint16_t width, uint16_t height)
{
	RECORD("%s, %i, %i", chan2str(ch), width, height);
	int ret, restart=0;
	struct v4l2_control control;

	TRACE("Setting resolution to %ux%u on %s channel\n", width, height,
			chan2str(ch));

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set the resolution for %s "
			"channel. The channel is not enabled.", chan2str(ch));

	/* SECS method */
	if(vstream->secs_enabled) {
		struct StreamFormat format;

		/* Fill with the current values */
		format.bStreamType = secs_format_mapping[vstream->cur_vfmt];
		format.dwFrameInterval = FRI(vstream->cur_framerate);
		format.dwBitrate = vstream->cur_bitrate;

		/* Set the width and height to the requested values */
		format.wWidth = width;
		format.wHeight = height;

		/* Trying setting the format with a SET PROBE */
		ret = set_skype_stream_control(ch, &format, 0);
		CHECK_ERROR(ret < 0, -1, "Unable to set/probe the resolution "
				"on %s", chan2str(ch));

		/* Check if the camera accepted the requested format */
		get_skype_stream_control(ch, &format, 0);
		CHECK_ERROR(format.wWidth != width || format.wHeight != height, -1,
			"The camera does not support %ix%i resolution on %s",
			width, height, chan2str(ch));

		/* Everything is OK, COMMIT the resolution */
		ret = set_skype_stream_control(ch, &format, 1);
		CHECK_ERROR(ret < 0, -1, "Unable to commit the resolution "
			"on %s", chan2str(ch));

		/* Update the cached stream format settings */
		vstream->cur_framerate = FRI(format.dwFrameInterval);
		vstream->cur_bitrate = format.dwBitrate;

		goto end_set_resolution;
	}

	/* Non SECS method */
	switch(vstream->cur_vfmt) {
	case VID_FORMAT_H264_RAW:
	case VID_FORMAT_H264_TS:
	case VID_FORMAT_H264_AAC_TS:
		control.id = V4L2_CID_XU_RESOLUTION2;
		control.value = (width << 16) + height;

		ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
		CHECK_ERROR(ret < 0, -1,
				"Unable to set the resolution on %s channel: "
				"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
				chan2str(ch), control.id, control.value,
				strerror(errno));
		break;
	case VID_FORMAT_MUX:
		if(ipcam_mode){
			uint32_t data = (width << 16) + height;
			ret = getset_mux_channel_param(ch, (void *)&data, sizeof(uint32_t),
							MUX_XU_RESOLUTION, 1);
			CHECK_ERROR(ret < 0, -1,
				"Unable to set the resolution on %s channel: "
				"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
				chan2str(ch), control.id, control.value,
				strerror(errno));
		}
		break;
	default:
		/* Stop the channel first if necessary */
		if(vstream->started) {
			ret = mxuvc_video_stop(ch);
			CHECK_ERROR(ret < 0, -1, "Failed to set the video format: "
					"could not stop video streaming on %s "
					"channel.", chan2str(ch));
			restart = 1;
		}

		vstream->fmt.fmt.pix.width = width;
		vstream->fmt.fmt.pix.height = height;
		ret = xioctl(vstream->fd, VIDIOC_S_FMT, &vstream->fmt);

		/* Restart the channel if necessary */
		if(restart) mxuvc_video_start(ch);

		/* Check for errors */
		CHECK_ERROR(ret == -1, -1, "Unable to set the resolution on %s channel: "
				"VIDIOC_S_FMT failed: %s", chan2str(ch),
				strerror(errno));
		break;
	}

end_set_resolution:
	vstream->cur_width = width;
	vstream->cur_height = height;

	return 0;
}

int mxuvc_video_get_resolution(video_channel_t ch, uint16_t *width, uint16_t *height)
{
	RECORD("%s, %p, %p", chan2str(ch), width, height);

	int ret = 0;
	struct v4l2_control control;
	uint32_t data = 0;

	TRACE("Getting resolution on %s channel\n", chan2str(ch));

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get the resolution for %s "
			"channel. The channel is not enabled.", chan2str(ch));

	if(vstream->secs_enabled) {
		struct StreamFormat format;
		ret = get_skype_stream_control(ch, &format, 0);
		CHECK_ERROR(ret < 0, -1, "Unable to get the resolution for %s.",
				chan2str(ch));
		*width = format.wWidth;
		*height = format.wHeight;
		return 0;
	}

	switch(vstream->cur_vfmt) {
	case VID_FORMAT_MUX:
		getset_mux_channel_param(ch, (void *)&data, sizeof(uint32_t),
						MUX_XU_RESOLUTION, 0);
		*width  = (uint16_t) ((data>>16) & 0xffff);
		*height = (uint16_t) (data & 0xffff);
		break;
	case VID_FORMAT_H264_RAW:
	case VID_FORMAT_H264_TS:
	case VID_FORMAT_H264_AAC_TS:
		control.id = V4L2_CID_XU_RESOLUTION2;
		ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
		CHECK_ERROR(ret < 0, -1,
			"Unable to get the resolution on %s channel: "
			"VIDIOC_G_CTRL failed (id = 0x%x): %s\n",
			chan2str(ch), control.id, strerror(errno));

		*width  = (uint16_t) ((control.value>>16) & 0xffff);
		*height = (uint16_t) (control.value & 0xffff);
		break;
	default:
		ret = xioctl(vstream->fd, VIDIOC_G_FMT, &vstream->fmt);
		CHECK_ERROR(ret == -1, -1,
			"Unable to get the resolution on %s channel: "
			"VIDIOC_G_FMT failed: %s", chan2str(ch),
			strerror(errno));
		*width = vstream->fmt.fmt.pix.width;
		*height = vstream->fmt.fmt.pix.height;
		break;
	}

	vstream->cur_width = *width;
	vstream->cur_height = *height;

	return 0;
}

int mxuvc_video_set_framerate(video_channel_t ch, uint32_t framerate)
{
	RECORD("%s, %i", chan2str(ch), framerate);
	int ret;
	struct v4l2_streamparm streamparam;

	TRACE("Setting the framerate to %i on %s channel.\n", framerate,
			chan2str(ch));

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set the framerate on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	if((vstream->cur_vfmt == VID_FORMAT_MUX) && (ipcam_mode)){
		uint32_t frminterval = FRI(framerate);
		getset_mux_channel_param(ch, (void *)&frminterval, sizeof(uint32_t),
						MUX_XU_FRAMEINTRVL, 1);
	}
	/* SECS method */
	else if (vstream->secs_enabled) {
		struct v4l2_control control;

		control.id = V4L2_CID_SKYPE_XU_FRAMEINTERVAL;
		control.value = FRI(framerate);
		ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
		CHECK_ERROR(ret < 0, -1, "Unable to set the framerate (%i).", ret);
	}
	/* Non SECS method */
	else {

		CLEAR(streamparam);
		streamparam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		streamparam.parm.capture.timeperframe.numerator = 1;
		streamparam.parm.capture.timeperframe.denominator = framerate;

		ret = ioctl(vstream->fd, VIDIOC_S_PARM, &streamparam);
		CHECK_ERROR(ret < 0, -1, "Unable to set the framerate (%i).", ret);
	}

	vstream->cur_framerate = framerate;

	return 0;
}


int mxuvc_video_get_framerate(video_channel_t ch, uint32_t *framerate)
{
	RECORD("%s, %p", chan2str(ch), framerate);
	int ret;
	struct v4l2_streamparm streamparam;
	struct v4l2_fract tpf;

	TRACE("Getting the framerate on %s channel.\n", chan2str(ch));

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get the framerate on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	if((vstream->cur_vfmt == VID_FORMAT_MUX) && (ipcam_mode)){
		uint32_t data;
		getset_mux_channel_param(ch, (void *)&data, sizeof(uint32_t),
						MUX_XU_FRAMEINTRVL, 0);
		*framerate = (float)FRR(data);
	}
	/* SECS method */
	else if (vstream->secs_enabled) {
		struct v4l2_control control;

		control.id = V4L2_CID_SKYPE_XU_FRAMEINTERVAL;
		ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);

		CHECK_ERROR(ret < 0, -1, "Unable to get the framerate (%i).", ret);
		*framerate = FRR(control.value);
	}
	/* Non SECS method */
	else {
		CLEAR(streamparam);
		streamparam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ret = ioctl(vstream->fd, VIDIOC_G_PARM, &streamparam);
		CHECK_ERROR(ret < 0, -1, "Unable to get the framerate (%i).", ret);

		tpf = streamparam.parm.capture.timeperframe;
		*framerate = (float)tpf.denominator/tpf.numerator + 0.5;
	}

	vstream->cur_framerate = *framerate;

	return 1;
}

int mxuvc_video_force_iframe(video_channel_t ch)
{
	RECORD("%s", chan2str(ch));
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
				"Unable to force I frame on %s channel: "
				"the channel is not enabled.", chan2str(ch));

	control.value = 1;

	if((vstream->cur_vfmt == VID_FORMAT_MUX) && (ipcam_mode)){
		getset_mux_channel_param(ch, (void *)&control.value, sizeof(uint32_t),
						MUX_XU_FORCE_I_FRAME, 1);
	} else {
		/* SECS method */
		if (vstream->secs_enabled)
			control.id = V4L2_CID_SKYPE_XU_GENERATEKEYFRAME;
		/* Non SECS method */
		else
			control.id = V4L2_CID_XU_FORCE_I_FRAME;

		ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
		CHECK_ERROR(ret < 0, -1,
			"Unable to force I frame on %s channel: "
			"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
			chan2str(ch), control.id, control.value,
			strerror(errno));
	}

	return 0;
}

static int set_pan(video_channel_t ch, int32_t pan)
{
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set Pan on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	struct v4l2_control control;

	control.id = V4L2_CID_PAN_ABSOLUTE;
	control.value = pan;
	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to set Pan to %i on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		control.value, chan2str(ch), control.id,
		control.value, strerror(errno));

	return 0;
}

static int set_tilt(video_channel_t ch, int32_t tilt)
{
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set Tilt on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	struct v4l2_control control;
	control.id = V4L2_CID_TILT_ABSOLUTE;
	control.value = tilt;
	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to set Tilt to %i on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		control.value, chan2str(ch), control.id,
		control.value, strerror(errno));

	return 0;
}


static int get_pan(video_channel_t ch, int32_t *pan)
{
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get Pan on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	struct v4l2_control control;

	control.id = V4L2_CID_PAN_ABSOLUTE;

	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
			"Unable to get Pan on %s channel: "
			"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
			chan2str(ch), control.id, control.value,
			strerror(errno));

	*pan = control.value;

	return 0;
}

static int get_tilt(video_channel_t ch, int32_t *tilt)
{
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get Tilt on %s channel: "
			"the channel is not enabled.", chan2str(ch));


	struct v4l2_control control;
	control.id = V4L2_CID_PAN_ABSOLUTE;

	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
			"Unable to get Tilt on %s channel: "
			"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
			chan2str(ch), control.id, control.value,
			strerror(errno));

	*tilt = control.value;

	return 0;
}


int mxuvc_video_set_pan(video_channel_t ch, int32_t pan)
{
	RECORD("%s, %i", chan2str(ch), pan);
	TRACE("Setting Pan to %d on %s channel\n", pan, chan2str(ch));

	return set_pan(ch, pan);
}
int mxuvc_video_set_tilt(video_channel_t ch, int32_t tilt)
{
	RECORD("%s, %i", chan2str(ch), tilt);
	TRACE("Setting Tilt to %d on %s channel\n", tilt, chan2str(ch));

	return set_tilt(ch, tilt);
}
int mxuvc_video_set_pantilt(video_channel_t ch, int32_t pan, int32_t tilt)
{
	RECORD("%s, %i, %i", chan2str(ch), pan, tilt);
	TRACE("Setting Pan/Tilt to %d/%d on %s channel\n",
			pan, tilt, chan2str(ch));

	if(set_pan(ch, pan) < 0)
		return -1;

	if(set_tilt(ch, tilt) < 0)
		return -1;

	return 0;
}

int mxuvc_video_get_pan(video_channel_t ch, int32_t *pan)
{
	RECORD("%s, %p", chan2str(ch), pan);
	TRACE("Getting Pan on %s channel\n", chan2str(ch));

	return get_pan(ch, pan);
}
int mxuvc_video_get_tilt(video_channel_t ch, int32_t *tilt)
{
	RECORD("%s, %p", chan2str(ch), tilt);
	TRACE("Getting Tilt on %s channel\n", chan2str(ch));

	return get_tilt(ch, tilt);
}
int mxuvc_video_get_pantilt(video_channel_t ch, int32_t *pan, int32_t *tilt)
{
	RECORD("%s, %p, %p", chan2str(ch), pan, tilt);
	TRACE("Getting Pan/Tilt on %s channel\n", chan2str(ch));

	if(get_pan(ch, pan) < 0)
		return -1;

	if(get_tilt(ch, tilt) < 0)
		return -1;

	return 0;
}


int mxuvc_video_set_wdr(video_channel_t ch, wdr_mode_t mode, uint8_t value)
{
	RECORD("%s, %i, %i", chan2str(ch), mode, value);
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set WDR mode to %d and strength to %d on %s channel: "
			"the channel is not enabled.", mode, value, chan2str(ch));

	control.id = V4L2_CID_PU_XU_ADAPTIVE_WDR_ENABLE;
	if ( mode == WDR_AUTO )
		control.value = 1;
	else
		control.value = 0;

	TRACE("Adaptive WDR Enable %i on %s channel\n", control.value, chan2str(ch));

	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to Adaptive WDR Enable on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		chan2str(ch), control.id, control.value,
		strerror(errno));

	control.id = V4L2_CID_PU_XU_WDR_STRENGTH;
	control.value = value;

	TRACE("WDR Strength %i on %s channel\n", control.value, chan2str(ch));

	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to WDR Strength on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		chan2str(ch), control.id, control.value,
		strerror(errno));

	return 0;

}

int mxuvc_video_get_wdr(video_channel_t ch, wdr_mode_t *mode, uint8_t *value)
{
	RECORD("%s, %p, %p", chan2str(ch), mode, value);
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get WDR mode and value on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	control.id = V4L2_CID_PU_XU_ADAPTIVE_WDR_ENABLE;

	TRACE("Getting Adaptive WDR Enable on %s channel\n", chan2str(ch));

	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to Get Adaptive WDR Enable on %s channel: "
		"VIDIOC_G_CTRL failed (id = 0x%x): %s\n",
		chan2str(ch), control.id, strerror(errno));

	if ( control.value )
		*mode = WDR_AUTO;
	else
		*mode = WDR_MANUAL;

	control.id = V4L2_CID_PU_XU_WDR_STRENGTH;

	TRACE("Getting WDR Strength on %s channel\n", chan2str(ch));

	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to Get WDR Strength on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x): %s\n",
		chan2str(ch), control.id, strerror(errno));

	*value = control.value;

	return 0;

}

int mxuvc_video_set_exp(video_channel_t ch, exp_set_t sel, uint16_t value)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set exposure on %s channel: "
			"the channel is not enabled\n", chan2str(ch));

	control.id = V4L2_CID_PU_XU_AUTO_EXPOSURE;
	if (sel == EXP_AUTO)
		control.value = 1;
	else
		control.value = 0;
	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Failed to %s auto exposure on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		(control.value == 1) ? "enable" : "disable", chan2str(ch),
		control.id, control.value, strerror(errno));

	control.id = V4L2_CID_PU_XU_EXPOSURE_TIME;
	control.value = value;
	TRACE("Set exposure time %d on %s channel\n", control.value, chan2str(ch));
	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to set exposure time on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		chan2str(ch), control.id, control.value,
		strerror(errno));

	return 0;
}

int mxuvc_video_get_exp(video_channel_t ch, exp_set_t *sel, uint16_t *value)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get exposure on %s channel: "
			"the channel is not enabled\n", chan2str(ch));

	control.id = V4L2_CID_PU_XU_AUTO_EXPOSURE;
	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to get exposure status on %s channel: "
		"VIDIOC_G_CTRL failed (id = 0x%x): %s\n",
		chan2str(ch), control.id, strerror(errno));
	if (control.value)
		*sel = EXP_AUTO;
	else
		*sel = EXP_MANUAL;

	control.id = V4L2_CID_PU_XU_EXPOSURE_TIME;
	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to get exposure time on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x): %s\n",
		chan2str(ch), control.id, strerror(errno));
	*value = control.value;

	return 0;
}

int mxuvc_video_set_zone_exp(video_channel_t ch, zone_exp_set_t sel, uint16_t value)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;
	exp_set_t exp_sel;
	uint16_t exp_val;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set zone based exposure on %s channel: "
			"the channel is not enabled\n", chan2str(ch));

	ret = mxuvc_video_get_exp(ch, &exp_sel, &exp_val);
	CHECK_ERROR(ret < 0, -1,
		"Unable to set zone based exposure on %s channel: "
		"checking auto exposure status failed\n",
		chan2str(ch));
	if (exp_sel == EXP_AUTO) {
		TRACE("Unable to set zone based exposure on %s channel: "
			"auto exposure is enabled on channel\n", chan2str(ch));
		return -EINVAL;
	}

	control.id = V4L2_CID_PU_XU_EXP_ZONE_SEL_ENABLE;
	if (sel == ZONE_EXP_ENABLE)
		control.value = 1;
	else
		control.value = 0;
	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Failed to enable zone based exposure on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		chan2str(ch), control.id, control.value,
		strerror(errno));

	control.id = V4L2_CID_PU_XU_EXP_ZONE_SEL;
	control.value = value;
	TRACE("Exposure zone %d on %s channel\n", control.value, chan2str(ch));
	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to exposure zone on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		chan2str(ch), control.id, control.value,
		strerror(errno));

	return 0;
}

int mxuvc_video_get_zone_exp(video_channel_t ch, zone_exp_set_t *sel, uint16_t *value)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;
	exp_set_t exp_sel;
	uint16_t exp_val;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get zone based exposure on %s channel: "
			"the channel is not enabled\n", chan2str(ch));

	ret = mxuvc_video_get_exp(ch, &exp_sel, &exp_val);
	CHECK_ERROR(ret < 0, -1,
		"Unable to get zone based exposure on %s channel: "
		"checking auto exposure status failed\n",
		chan2str(ch));
	if (exp_sel == EXP_AUTO) {
		TRACE("Unable to get zone based exposure on %s channel: "
			"auto exposure is enabled on channel\n", chan2str(ch));
		return -EINVAL;
	}

	control.id = V4L2_CID_PU_XU_EXP_ZONE_SEL_ENABLE;
	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to get zone based exposure status on %s channel: "
		"VIDIOC_G_CTRL failed (id = 0x%x): %s\n",
		chan2str(ch), control.id, strerror(errno));
	if (control.value)
		*sel = ZONE_EXP_ENABLE;
	else
		*sel = ZONE_EXP_DISABLE;

	control.id = V4L2_CID_PU_XU_EXP_ZONE_SEL;
	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to get exposure zone on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x): %s\n",
		chan2str(ch), control.id, strerror(errno));
	*value = control.value;

	return 0;
}

int mxuvc_video_set_crop(video_channel_t ch, crop_info_t *info)
{
	RECORD("%s, %p", chan2str(ch), info);
	int ret, i;
        struct v4l2_ext_controls ctrls;
        struct v4l2_ext_control ctrl[5];

	assert(info != NULL);
	TRACE("Setting crop to {en:%i, w:%i, h:%i, x:%i, y:%i}) on %s channel.\n",
			info->enable, info->width, info->height, info->x, info->y,
			chan2str(ch));

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set the crop window on %s channel: "
			"the channel is not enabled.", chan2str(ch));

        memset(&ctrls, 0, sizeof(ctrls));
        ctrls.count = 5;
        ctrls.controls = ctrl;

        memset(ctrl, 0, sizeof(ctrl));
        for(i = 0; i < 5; i++)
                ctrl[i].id = V4L2_CID_PU_XU_CROP+i;
        ctrl[0].value = info->enable;
        ctrl[1].value = info->width;
        ctrl[2].value = info->height;
        ctrl[3].value = info->x;
        ctrl[4].value = info->y;

        ret = ioctl(vstream->fd, VIDIOC_S_EXT_CTRLS, &ctrls);
	CHECK_ERROR(ret < 0, -1,
		"Unable to set the crop window on %s channel: "
		"VIDIOC_S_EXT_CTRLS failed: %s\n",
		chan2str(ch), strerror(errno));

	return 0;
}

int mxuvc_video_get_crop(video_channel_t ch, crop_info_t *info)
{
	RECORD("%s, %p", chan2str(ch), info);
	int ret, i;
        struct v4l2_ext_controls ctrls;
        struct v4l2_ext_control ctrl[5];

	assert(info != NULL);
	TRACE("Getting crop on %s channel.\n", chan2str(ch));

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get the crop info on %s channel: "
			"the channel is not enabled.", chan2str(ch));

        memset(&ctrls, 0, sizeof(ctrls));
        ctrls.count = 5;
        ctrls.controls = ctrl;

        memset(ctrl, 0, sizeof(ctrl));
        for(i = 0; i < 5; i++)
                ctrl[i].id = V4L2_CID_PU_XU_CROP+i;

        ret = ioctl(vstream->fd, VIDIOC_G_EXT_CTRLS, &ctrls);
	CHECK_ERROR(ret < 0, -1,
		"Unable to get the crop window on %s channel: "
		"VIDIOC_G_EXT_CTRLS failed: %s\n",
		chan2str(ch), strerror(errno));

	info->enable = ctrl[0].value;
	info->width  = ctrl[1].value;
	info->height = ctrl[2].value;
	info->x      = ctrl[3].value;
	info->y      = ctrl[4].value;

	return 0;
}

int mxuvc_video_set_nf(video_channel_t ch, noise_filter_mode_t sel, uint16_t value)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;
	
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set ANF mode to %d and strength to %d on %s channel: "
			"the channel is not enabled.", sel, value, chan2str(ch));

	if ( sel == NF_MODE_AUTO ){
		control.id = V4L2_CID_PU_XU_ANF_ENABLE;
		control.value = 1;
	} else {
		control.id = V4L2_CID_PU_XU_ANF_ENABLE;
		control.value = 0;
		
		ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
		CHECK_ERROR(ret < 0, -1,
		"Unable to set NF Enable on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		chan2str(ch), control.id, control.value,
		strerror(errno));

		control.id = V4L2_CID_PU_XU_NF_STRENGTH;
		control.value = value;	
	}

	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to set NF Enable on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		chan2str(ch), control.id, control.value,
		strerror(errno));
	
	return 0;
}

int mxuvc_video_get_nf(video_channel_t ch, noise_filter_mode_t *sel, uint16_t *value)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;
	
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get NF value on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	control.id = V4L2_CID_PU_XU_ANF_ENABLE;
	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to Get ANF value on %s channel: "
		"VIDIOC_G_CTRL failed (id = 0x%x): %s\n",
		chan2str(ch), control.id, strerror(errno));

	if ( control.value )
		*sel = NF_MODE_AUTO;
	else
		*sel = NF_MODE_MANUAL;

	if( *sel == NF_MODE_MANUAL){
		control.id = V4L2_CID_PU_XU_NF_STRENGTH;
		ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
		CHECK_ERROR(ret < 0, -1,
			"Unable to Get Adaptive NF Strength on %s channel: "
			"VIDIOC_G_CTRL failed (id = 0x%x): %s\n",
			chan2str(ch), control.id, strerror(errno));
		*value = control.value;
	}	

	return 0;
}

int mxuvc_video_set_wb(video_channel_t ch, white_balance_mode_t sel, uint16_t value)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;
	
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set WB mode to %d and strength to %d on %s channel: "
			"the channel is not enabled.", sel, value, chan2str(ch));

	if ( sel == WB_MODE_AUTO ){
		control.id = V4L2_CID_PU_XU_AUTO_WHITE_BAL;
		control.value = 1;
	} else {
		control.id = V4L2_CID_PU_XU_AUTO_WHITE_BAL;
		control.value = 0;
		
		ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
		CHECK_ERROR(ret < 0, -1,
		"Unable to set Auto WB on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		chan2str(ch), control.id, control.value,
		strerror(errno));

		control.id = V4L2_CID_PU_XU_WHITE_BAL_TEMP;
		control.value = value;	
	}

	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to set WBT on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		chan2str(ch), control.id, control.value,
		strerror(errno));

	return 0;
}

int mxuvc_video_get_wb(video_channel_t ch, white_balance_mode_t *sel, uint16_t *value)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;
	
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get WB value on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	control.id = V4L2_CID_PU_XU_AUTO_WHITE_BAL;
	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to Get ANF value on %s channel: "
		"VIDIOC_G_CTRL failed (id = 0x%x): %s\n",
		chan2str(ch), control.id, strerror(errno));

	if( control.value )	
		*sel = WB_MODE_AUTO;
	else 
		*sel = WB_MODE_MANUAL;

	if( *sel == WB_MODE_MANUAL )
	{
		control.id = V4L2_CID_PU_XU_WHITE_BAL_TEMP;
		ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
		CHECK_ERROR(ret < 0, -1,
			"Unable to Get WBT value on %s channel: "
			"VIDIOC_G_CTRL failed (id = 0x%x): %s\n",
			chan2str(ch), control.id, strerror(errno));
		*value = control.value;
	}

	return 0;
}

int mxuvc_video_set_pwr_line_freq(video_channel_t ch, pwr_line_freq_mode_t mode)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;
	
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set pwr line freq to %d on %s channel: "
			"the channel is not enabled.", mode, chan2str(ch));
	
	control.id = V4L2_CID_POWER_LINE_FREQUENCY;
	control.value = mode;
	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to set pwr line freq on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		chan2str(ch), control.id, control.value,
		strerror(errno));	

	return 0;
}

int mxuvc_video_get_pwr_line_freq(video_channel_t ch, pwr_line_freq_mode_t *mode)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;
	
	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get pwr line freq on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	control.id = V4L2_CID_POWER_LINE_FREQUENCY;
	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to fet pwr line freq on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x): %s\n",
		chan2str(ch), control.id, strerror(errno));
	*mode = control.value;
	
	return 0;
}

int mxuvc_video_set_zone_wb(video_channel_t ch, zone_wb_set_t sel, uint16_t value)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set zone wb on %s channel: "
			"the channel is not enabled.", chan2str(ch));

	if ( sel == ZONE_WB_ENABLE )
		control.value = 1;
	else
		control.value = 0;
		
	control.id = V4L2_CID_PU_XU_WB_ZONE_SEL_ENABLE;
	ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
		"Unable to enable zone wb on %s channel: "
		"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
		chan2str(ch), control.id, control.value, strerror(errno));
		
	if ( sel == ZONE_WB_ENABLE ){
		control.id = V4L2_CID_PU_XU_WB_ZONE_SEL;
		control.value =  value;
		ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
		CHECK_ERROR(ret < 0, -1,
			"Unable to set zone wb on %s channel: "
			"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",
			chan2str(ch), control.id, control.value, strerror(errno));
	}

	return 0;
}

int mxuvc_video_get_zone_wb(video_channel_t ch, zone_wb_set_t *sel, uint16_t *value)
{
	struct v4l2_control control;
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get zone wb on %s channel: "
			"the channel is not enabled.", chan2str(ch));	

	control.id = V4L2_CID_PU_XU_WB_ZONE_SEL_ENABLE;
	ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
	CHECK_ERROR(ret < 0, -1,
			"Unable to get zone wb on %s channel: "
			"VIDIOC_S_CTRL failed (id = 0x%x): %s\n",
			chan2str(ch), control.id, strerror(errno));
	*sel =  control.value;

	if ( *sel == ZONE_WB_ENABLE ){
		control.id = V4L2_CID_PU_XU_WB_ZONE_SEL;
		ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);
		CHECK_ERROR(ret < 0, -1,
			"Unable to get zone wb on %s channel: "
			"VIDIOC_S_CTRL failed (id = 0x%x): %s\n",
			chan2str(ch), control.id, strerror(errno));
		*value = control.value;
	}
	return 0;
	
}

int mxuvc_video_get_channel_info(video_channel_t ch, video_channel_info_t *info)
{
	struct video_stream *vstream;
	uint32_t data = 0;

	vstream = &video_stream[ch];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get info for %s channel: "
			"the channel is not enabled\n", chan2str(ch));		

	if((vstream->cur_vfmt == VID_FORMAT_MUX) && ipcam_mode){
		//resolution	
		getset_mux_channel_param(ch, (void *)&data, sizeof(uint32_t),
						MUX_XU_RESOLUTION, 0);
		info->width  = (uint16_t) ((data>>16) & 0xffff);
		info->height = (uint16_t) (data & 0xffff);
		//format
		uint32_t fourcc;
		getset_mux_channel_param(ch, (void *)&fourcc, sizeof(uint32_t),
						MUX_XU_CHTYPE, 0);
		video_format_t format;
		fourcc2vidformat(fourcc, &format);
		info->format = format;
		//frame rate
		getset_mux_channel_param(ch, (void *)&data, sizeof(uint32_t),
						MUX_XU_FRAMEINTRVL, 0);
		info->framerate = (float)FRR(data);
		if(format == VID_FORMAT_H264_RAW ||
			format == VID_FORMAT_H264_TS)
		{
			//gop
			getset_mux_channel_param(ch, (void *)&data, sizeof(uint32_t),
							MUX_XU_GOP_LENGTH, 0);
			info->goplen = 	data;

			//h264 profile
			getset_mux_channel_param(ch, (void *)&data, sizeof(uint32_t),
							MUX_XU_AVC_PROFILE, 0);
			info->profile = data;
				
			//bitrate
			getset_mux_channel_param(ch, (void *)&data, sizeof(uint32_t),
							MUX_XU_BITRATE, 0);
			info->bitrate = data;
			//compression_quality TBD
			//getset_mux_channel_param(ch, (void *)&data, sizeof(uint32_t),
			//				MUX_XU_COMPRESSION_Q, 0);
			//info->compression_quality = data;
		}

	} else if (ipcam_mode) {
		/* raw stream */
		mxuvc_video_get_resolution(ch, &info->width, &info->height);
		info->format = vstream->cur_vfmt;
		mxuvc_video_get_framerate(ch, &info->framerate);
	}
	return 0;
}

static int getset_mux_channel_param(video_channel_t ch,
				void *data,
				uint32_t data_size,
				enum MUX_XU_CTRL xu_name,
				uint32_t set)
{
	static uint32_t muxch_v4l2_mapping[MUX_XU_NUM_CTRLS+1][NUM_MUX_VID_CHANNELS] = {
	[MUX_XU_RESOLUTION][CH1]    = V4L2_CID_MUX_CH1_XU_RESOLUTION,
	[MUX_XU_RESOLUTION][CH2]    = V4L2_CID_MUX_CH2_XU_RESOLUTION,
	[MUX_XU_RESOLUTION][CH3]    = V4L2_CID_MUX_CH3_XU_RESOLUTION,
	[MUX_XU_RESOLUTION][CH4]    = V4L2_CID_MUX_CH4_XU_RESOLUTION,
	[MUX_XU_RESOLUTION][CH5]    = V4L2_CID_MUX_CH5_XU_RESOLUTION,
	[MUX_XU_RESOLUTION][CH6]    = V4L2_CID_MUX_CH6_XU_RESOLUTION,
	[MUX_XU_RESOLUTION][CH7]    = V4L2_CID_MUX_CH7_XU_RESOLUTION,	

	[MUX_XU_FRAMEINTRVL][CH1]   = V4L2_CID_MUX_CH1_XU_FRAMEINTRVL,
	[MUX_XU_FRAMEINTRVL][CH2]   = V4L2_CID_MUX_CH2_XU_FRAMEINTRVL,
	[MUX_XU_FRAMEINTRVL][CH3]   = V4L2_CID_MUX_CH3_XU_FRAMEINTRVL,
	[MUX_XU_FRAMEINTRVL][CH4]   = V4L2_CID_MUX_CH4_XU_FRAMEINTRVL,
	[MUX_XU_FRAMEINTRVL][CH5]   = V4L2_CID_MUX_CH5_XU_FRAMEINTRVL,
	[MUX_XU_FRAMEINTRVL][CH6]   = V4L2_CID_MUX_CH6_XU_FRAMEINTRVL,
	[MUX_XU_FRAMEINTRVL][CH7]   = V4L2_CID_MUX_CH7_XU_FRAMEINTRVL,

	[MUX_XU_ZOOM][CH1]          = V4L2_CID_MUX_CH1_XU_ZOOM,
	[MUX_XU_ZOOM][CH2]          = V4L2_CID_MUX_CH2_XU_ZOOM,
	[MUX_XU_ZOOM][CH3]          = V4L2_CID_MUX_CH3_XU_ZOOM,
	[MUX_XU_ZOOM][CH4]          = V4L2_CID_MUX_CH4_XU_ZOOM,
	[MUX_XU_ZOOM][CH5]          = V4L2_CID_MUX_CH5_XU_ZOOM,
	[MUX_XU_ZOOM][CH6]          = V4L2_CID_MUX_CH6_XU_ZOOM,
	[MUX_XU_ZOOM][CH7]          = V4L2_CID_MUX_CH7_XU_ZOOM,

	[MUX_XU_BITRATE][CH1]       = V4L2_CID_MUX_CH1_XU_BITRATE,
	[MUX_XU_BITRATE][CH2]       = V4L2_CID_MUX_CH2_XU_BITRATE,
	[MUX_XU_BITRATE][CH3]       = V4L2_CID_MUX_CH3_XU_BITRATE,
	[MUX_XU_BITRATE][CH4]       = V4L2_CID_MUX_CH4_XU_BITRATE,
	[MUX_XU_BITRATE][CH5]       = V4L2_CID_MUX_CH5_XU_BITRATE,
	[MUX_XU_BITRATE][CH6]       = V4L2_CID_MUX_CH6_XU_BITRATE,
	[MUX_XU_BITRATE][CH7]       = V4L2_CID_MUX_CH7_XU_BITRATE,

	[MUX_XU_FORCE_I_FRAME][CH1] = V4L2_CID_MUX_CH1_XU_FORCE_I_FRAME,
	[MUX_XU_FORCE_I_FRAME][CH2] = V4L2_CID_MUX_CH2_XU_FORCE_I_FRAME,
	[MUX_XU_FORCE_I_FRAME][CH3] = V4L2_CID_MUX_CH3_XU_FORCE_I_FRAME,
	[MUX_XU_FORCE_I_FRAME][CH4] = V4L2_CID_MUX_CH4_XU_FORCE_I_FRAME,
	[MUX_XU_FORCE_I_FRAME][CH5] = V4L2_CID_MUX_CH5_XU_FORCE_I_FRAME,
	[MUX_XU_FORCE_I_FRAME][CH6] = V4L2_CID_MUX_CH6_XU_FORCE_I_FRAME,
	[MUX_XU_FORCE_I_FRAME][CH7] = V4L2_CID_MUX_CH7_XU_FORCE_I_FRAME,

	[MUX_XU_VUI_ENABLE][CH1]   = V4L2_CID_MUX_CH1_XU_VUI_ENABLE,
	[MUX_XU_VUI_ENABLE][CH2]   = V4L2_CID_MUX_CH2_XU_VUI_ENABLE,
	[MUX_XU_VUI_ENABLE][CH3]   = V4L2_CID_MUX_CH3_XU_VUI_ENABLE,
	[MUX_XU_VUI_ENABLE][CH4]   = V4L2_CID_MUX_CH4_XU_VUI_ENABLE,
	[MUX_XU_VUI_ENABLE][CH5]   = V4L2_CID_MUX_CH5_XU_VUI_ENABLE,
	[MUX_XU_VUI_ENABLE][CH6]   = V4L2_CID_MUX_CH6_XU_VUI_ENABLE,
	[MUX_XU_VUI_ENABLE][CH7]   = V4L2_CID_MUX_CH7_XU_VUI_ENABLE,

	[MUX_XU_COMPRESSION_Q][CH1] = V4L2_CID_MUX_CH1_XU_COMPRESSION_Q,
	[MUX_XU_COMPRESSION_Q][CH2] = V4L2_CID_MUX_CH2_XU_COMPRESSION_Q,
	[MUX_XU_COMPRESSION_Q][CH3] = V4L2_CID_MUX_CH3_XU_COMPRESSION_Q,
	[MUX_XU_COMPRESSION_Q][CH4] = V4L2_CID_MUX_CH4_XU_COMPRESSION_Q,
	[MUX_XU_COMPRESSION_Q][CH5] = V4L2_CID_MUX_CH5_XU_COMPRESSION_Q,
	[MUX_XU_COMPRESSION_Q][CH6] = V4L2_CID_MUX_CH6_XU_COMPRESSION_Q,
	[MUX_XU_COMPRESSION_Q][CH7] = V4L2_CID_MUX_CH7_XU_COMPRESSION_Q,

	[MUX_XU_CHCOUNT][CH1]		= V4L2_CID_MUX_CH1_XU_CHCOUNT,

	[MUX_XU_CHTYPE][CH1]		= V4L2_CID_MUX_CH1_XU_CHTYPE,
	[MUX_XU_CHTYPE][CH2]		= V4L2_CID_MUX_CH2_XU_CHTYPE,
	[MUX_XU_CHTYPE][CH3]		= V4L2_CID_MUX_CH3_XU_CHTYPE,
	[MUX_XU_CHTYPE][CH4]		= V4L2_CID_MUX_CH4_XU_CHTYPE,
	[MUX_XU_CHTYPE][CH5]		= V4L2_CID_MUX_CH5_XU_CHTYPE,
	[MUX_XU_CHTYPE][CH6]		= V4L2_CID_MUX_CH6_XU_CHTYPE,
	[MUX_XU_CHTYPE][CH7]		= V4L2_CID_MUX_CH7_XU_CHTYPE,

	[MUX_XU_GOP_LENGTH][CH1]	= V4L2_CID_MUX_CH1_XU_GOP_LENGTH,
	[MUX_XU_GOP_LENGTH][CH2]	= V4L2_CID_MUX_CH2_XU_GOP_LENGTH,
	[MUX_XU_GOP_LENGTH][CH3]	= V4L2_CID_MUX_CH3_XU_GOP_LENGTH,
	[MUX_XU_GOP_LENGTH][CH4]	= V4L2_CID_MUX_CH4_XU_GOP_LENGTH,
	[MUX_XU_GOP_LENGTH][CH5]	= V4L2_CID_MUX_CH5_XU_GOP_LENGTH,
	[MUX_XU_GOP_LENGTH][CH6]	= V4L2_CID_MUX_CH6_XU_GOP_LENGTH,
	[MUX_XU_GOP_LENGTH][CH7]	= V4L2_CID_MUX_CH7_XU_GOP_LENGTH,

	[MUX_XU_AVC_PROFILE][CH1]	= V4L2_CID_MUX_CH1_XU_AVC_PROFILE,
	[MUX_XU_AVC_PROFILE][CH2]	= V4L2_CID_MUX_CH2_XU_AVC_PROFILE,
	[MUX_XU_AVC_PROFILE][CH3]	= V4L2_CID_MUX_CH3_XU_AVC_PROFILE,
	[MUX_XU_AVC_PROFILE][CH4]	= V4L2_CID_MUX_CH4_XU_AVC_PROFILE,
	[MUX_XU_AVC_PROFILE][CH5]	= V4L2_CID_MUX_CH5_XU_AVC_PROFILE,

	[MUX_XU_AVC_MAX_FRAME_SIZE][CH1]   = V4L2_CID_MUX_CH1_XU_AVC_MAX_FRAME_SIZE,
	[MUX_XU_AVC_MAX_FRAME_SIZE][CH2]   = V4L2_CID_MUX_CH2_XU_AVC_MAX_FRAME_SIZE,
	[MUX_XU_AVC_MAX_FRAME_SIZE][CH3]   = V4L2_CID_MUX_CH3_XU_AVC_MAX_FRAME_SIZE,
	[MUX_XU_AVC_MAX_FRAME_SIZE][CH4]   = V4L2_CID_MUX_CH4_XU_AVC_MAX_FRAME_SIZE,
	[MUX_XU_AVC_MAX_FRAME_SIZE][CH5]   = V4L2_CID_MUX_CH5_XU_AVC_MAX_FRAME_SIZE,

	[MUX_XU_START_CHANNEL][CH1] = V4L2_CID_MUX_XU_START_CHANNEL,
	[MUX_XU_START_CHANNEL][CH2] = V4L2_CID_MUX_XU_START_CHANNEL,
	[MUX_XU_START_CHANNEL][CH3] = V4L2_CID_MUX_XU_START_CHANNEL,
	[MUX_XU_START_CHANNEL][CH4] = V4L2_CID_MUX_XU_START_CHANNEL,
	[MUX_XU_START_CHANNEL][CH5] = V4L2_CID_MUX_XU_START_CHANNEL,
	[MUX_XU_START_CHANNEL][CH6] = V4L2_CID_MUX_XU_START_CHANNEL,
	[MUX_XU_START_CHANNEL][CH7] = V4L2_CID_MUX_XU_START_CHANNEL,

	[MUX_XU_STOP_CHANNEL][CH1] = V4L2_CID_MUX_XU_STOP_CHANNEL, 
	[MUX_XU_STOP_CHANNEL][CH2] = V4L2_CID_MUX_XU_STOP_CHANNEL,
	[MUX_XU_STOP_CHANNEL][CH3] = V4L2_CID_MUX_XU_STOP_CHANNEL,
	[MUX_XU_STOP_CHANNEL][CH4] = V4L2_CID_MUX_XU_STOP_CHANNEL,
	[MUX_XU_STOP_CHANNEL][CH5] = V4L2_CID_MUX_XU_STOP_CHANNEL,
	[MUX_XU_STOP_CHANNEL][CH6] = V4L2_CID_MUX_XU_STOP_CHANNEL,
	[MUX_XU_STOP_CHANNEL][CH7] = V4L2_CID_MUX_XU_STOP_CHANNEL,

	[MUX_XU_AVC_LEVEL][CH1] = V4L2_CID_MUX_CH1_XU_AVC_LEVEL, 
	[MUX_XU_AVC_LEVEL][CH2] = V4L2_CID_MUX_CH2_XU_AVC_LEVEL,
	[MUX_XU_AVC_LEVEL][CH3] = V4L2_CID_MUX_CH3_XU_AVC_LEVEL,
	[MUX_XU_AVC_LEVEL][CH4] = V4L2_CID_MUX_CH4_XU_AVC_LEVEL,
	[MUX_XU_AVC_LEVEL][CH5] = V4L2_CID_MUX_CH5_XU_AVC_LEVEL,
	[MUX_XU_AVC_LEVEL][CH6] = V4L2_CID_MUX_CH6_XU_AVC_LEVEL,
	[MUX_XU_AVC_LEVEL][CH7] = V4L2_CID_MUX_CH7_XU_AVC_LEVEL,
	
	[MUX_XU_VFLIP][CH1] = V4L2_CID_MUX_CH1_XU_VFLIP,

	[MUX_XU_AUDIO_BITRATE][CH1] = V4L2_CID_MUX_CH1_XU_AUDIO_BITRATE,

	[MUX_XU_PIC_TIMING_ENABLE][CH1] = V4L2_CID_MUX_CH1_XU_PIC_TIMING_ENABLE, 
 	[MUX_XU_PIC_TIMING_ENABLE][CH2] = V4L2_CID_MUX_CH2_XU_PIC_TIMING_ENABLE,
	[MUX_XU_PIC_TIMING_ENABLE][CH3] = V4L2_CID_MUX_CH3_XU_PIC_TIMING_ENABLE,
	[MUX_XU_PIC_TIMING_ENABLE][CH4] = V4L2_CID_MUX_CH4_XU_PIC_TIMING_ENABLE,
	[MUX_XU_PIC_TIMING_ENABLE][CH5] = V4L2_CID_MUX_CH5_XU_PIC_TIMING_ENABLE,
	[MUX_XU_PIC_TIMING_ENABLE][CH6] = V4L2_CID_MUX_CH6_XU_PIC_TIMING_ENABLE,
	[MUX_XU_PIC_TIMING_ENABLE][CH7] = V4L2_CID_MUX_CH7_XU_PIC_TIMING_ENABLE,

	[MUX_XU_GOP_HIERARCHY_LEVEL][CH1] = V4L2_CID_MUX_CH1_XU_GOP_HIERARCHY_LEVEL, 
 	[MUX_XU_GOP_HIERARCHY_LEVEL][CH2] = V4L2_CID_MUX_CH2_XU_GOP_HIERARCHY_LEVEL,
	[MUX_XU_GOP_HIERARCHY_LEVEL][CH3] = V4L2_CID_MUX_CH3_XU_GOP_HIERARCHY_LEVEL,
	[MUX_XU_GOP_HIERARCHY_LEVEL][CH4] = V4L2_CID_MUX_CH4_XU_GOP_HIERARCHY_LEVEL,
	[MUX_XU_GOP_HIERARCHY_LEVEL][CH5] = V4L2_CID_MUX_CH5_XU_GOP_HIERARCHY_LEVEL,
	[MUX_XU_GOP_HIERARCHY_LEVEL][CH6] = V4L2_CID_MUX_CH6_XU_GOP_HIERARCHY_LEVEL,
	[MUX_XU_GOP_HIERARCHY_LEVEL][CH7] = V4L2_CID_MUX_CH7_XU_GOP_HIERARCHY_LEVEL,
	};

	int ret;
	struct v4l2_control control;

	struct video_stream *vstream;
	vstream = &video_stream[ch];
	control.id = muxch_v4l2_mapping[xu_name][ch];

	if (set == 1){
		if(data_size == 1)
			control.value = *(uint8_t *)data;
		else if (data_size == 2)
			control.value = *(uint16_t *)data;
		else if (data_size == 4)
			control.value = *(uint32_t *)data;

		ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);
		CHECK_ERROR(ret < 0, -1,
			"On %s channel: "
			"VIDIOC_S_CTRL failed (id = 0x%x, value = %d): %s\n",
			chan2str(ch), control.id, control.value,
			strerror(errno));

	} else { //get
		ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);

		if(data_size == 1)
			*(uint8_t *)data = (uint8_t)control.value;
		else if (data_size == 2)
			*(uint16_t *)data = (uint16_t)control.value;
		else if (data_size == 4)
			*(uint32_t *)data = (uint32_t)control.value;
		CHECK_ERROR(ret < 0, -1,
			"On %s channel: "
			"VIDIOC_G_CTRL failed (id = 0x%x, value = %d): %s\n",
			chan2str(ch), control.id, control.value,
			strerror(errno));
	}

	return 0;
}

int mxuvc_audio_get_bitrate(uint32_t *value)
{
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[CH1];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to get audio bitrate on %s channel: "
			"the channel is not enabled.", chan2str(CH1));

	if((vstream->cur_vfmt == VID_FORMAT_MUX) && ipcam_mode){
		ret = getset_mux_channel_param(CH1, (void *)value,
				sizeof(uint32_t),
				MUX_XU_AUDIO_BITRATE,
				0);
		CHECK_ERROR(ret < 0, -1,
			"Unable to get audio bitrate on %s channel.", chan2str(CH1));
	}

	return 0;
}

int mxuvc_audio_set_bitrate(uint32_t value)
{
	struct video_stream *vstream;
	int ret;

	vstream = &video_stream[CH1];
	CHECK_ERROR(vstream->enabled == 0, -1,
			"Unable to set audio bitrate on %s channel: "
			"the channel is not enabled.", chan2str(CH1));

	if((vstream->cur_vfmt == VID_FORMAT_MUX) && ipcam_mode){
		ret = getset_mux_channel_param(CH1, (void *)&value,
				sizeof(uint32_t),
				MUX_XU_AUDIO_BITRATE,
				1);
		CHECK_ERROR(ret < 0, -1,
			"Unable to set audio bitrate on %s channel.", chan2str(CH1));
	}

	return 0;

}

#define DECLARE_SET(ctrl, ctrl_id, size_type) \
	int mxuvc_video_set_##ctrl (video_channel_t ch, size_type value)\
	{\
	RECORD("%s, %i", chan2str(ch), value);\
	struct v4l2_control control;\
	struct video_stream *vstream;\
	int ret, id;\
	\
	control.id = ctrl_id;\
	control.value = value;\
	TRACE("Setting " #ctrl " to %d on %s channel\n", control.value,\
			chan2str(ch));\
	\
	vstream = &video_stream[ch];\
	CHECK_ERROR(vstream->enabled == 0, -1,\
			"Unable to set the " #ctrl " on %s channel: "\
			"the channel is not enabled.", chan2str(ch));\
	\
	if((vstream->cur_vfmt == VID_FORMAT_MUX) && (ipcam_mode) && \
		(ctrl_id == V4L2_CID_DIGITIAL_MULTIPLIER || \
		ctrl_id == V4L2_CID_XU_BITRATE		 || \
		ctrl_id == V4L2_CID_XU_GOP_LENGTH 	 || \
		ctrl_id == MUX_XU_VUI_ENABLE		 || \
		ctrl_id == MUX_XU_COMPRESSION_Q		 || \
		ctrl_id == V4L2_CID_XU_AVC_PROFILE	 || \
		ctrl_id == MUX_XU_AVC_LEVEL		 || \
		ctrl_id == MUX_XU_PIC_TIMING_ENABLE	 || \
		ctrl_id == MUX_XU_GOP_HIERARCHY_LEVEL    || \
		ctrl_id == V4L2_CID_XU_MAX_FRAME_SIZE))	    \
	{ \
		if(ctrl_id == V4L2_CID_DIGITIAL_MULTIPLIER)	\
			id = MUX_XU_ZOOM; 			\
		else if(ctrl_id == V4L2_CID_XU_BITRATE)		\
			id = MUX_XU_BITRATE; 			\
		else if(ctrl_id == V4L2_CID_XU_GOP_LENGTH) 	\
			id = MUX_XU_GOP_LENGTH;			\
		else if(ctrl_id == V4L2_CID_XU_AVC_PROFILE)	\
			id = MUX_XU_AVC_PROFILE; 		\
		else if(ctrl_id == V4L2_CID_XU_MAX_FRAME_SIZE)	\
			id = MUX_XU_AVC_MAX_FRAME_SIZE;		\
		else						\
			id = ctrl_id;				\
		getset_mux_channel_param(ch, 			\
				(void *)&value, 		\
				sizeof(size_type), 		\
				id,				\
				1); 				\
	} else { \
		ret = ioctl(vstream->fd, VIDIOC_S_CTRL, &control);\
		CHECK_ERROR(ret < 0, -1, \
			"Unable to set the " #ctrl " to %i on %s channel: "\
			"VIDIOC_S_CTRL failed (id = 0x%x, value = %i): %s\n",\
			control.value, chan2str(ch), control.id, control.value,\
			strerror(errno));\
	} \
	return 0;\
}

#define DECLARE_GET(ctrl, ctrl_id, size_type) \
	int mxuvc_video_get_##ctrl(video_channel_t ch, size_type *value)\
	{\
	RECORD("%s, %p", chan2str(ch), value);\
	struct v4l2_control control;\
	struct video_stream *vstream;\
	int ret, id;\
	\
	control.id = ctrl_id;\
	TRACE("Getting " #ctrl " on %s channel\n", chan2str(ch));\
	\
	vstream = &video_stream[ch];\
	CHECK_ERROR(vstream->enabled == 0, -1,\
			"Unable to get the " #ctrl "on %s channel: "\
			"the channel is not enabled.", chan2str(ch));\
	\
	if((vstream->cur_vfmt == VID_FORMAT_MUX) && (ipcam_mode) && \
		(ctrl_id == V4L2_CID_DIGITIAL_MULTIPLIER || \
		ctrl_id == V4L2_CID_XU_BITRATE 		 || \
		ctrl_id == V4L2_CID_XU_GOP_LENGTH 	 || \
		ctrl_id == MUX_XU_VUI_ENABLE 		 || \
		ctrl_id == MUX_XU_COMPRESSION_Q		 || \
		ctrl_id == V4L2_CID_XU_AVC_PROFILE	 || \
		ctrl_id == MUX_XU_AVC_LEVEL		 || \
		ctrl_id == MUX_XU_PIC_TIMING_ENABLE	 || \
		ctrl_id == MUX_XU_GOP_HIERARCHY_LEVEL    || \
		ctrl_id == V4L2_CID_XU_MAX_FRAME_SIZE))	    \
	{ \
		if(ctrl_id == V4L2_CID_DIGITIAL_MULTIPLIER)	\
			id = MUX_XU_ZOOM; 			\
		else if(ctrl_id == V4L2_CID_XU_BITRATE)		\
			id = MUX_XU_BITRATE; 			\
		else if(ctrl_id == V4L2_CID_XU_GOP_LENGTH) 	\
			id = MUX_XU_GOP_LENGTH;			\
		else if(ctrl_id == V4L2_CID_XU_AVC_PROFILE)	\
			id = MUX_XU_AVC_PROFILE; 		\
		else if(ctrl_id == V4L2_CID_XU_MAX_FRAME_SIZE)	\
			id = MUX_XU_AVC_MAX_FRAME_SIZE;		\
		else 						\
			id = ctrl_id;				\
		getset_mux_channel_param(ch, 			\
				(void *)value, 			\
				sizeof(size_type), 		\
				id,				\
				0); 				\
	} else { 						\
		ret = ioctl(vstream->fd, VIDIOC_G_CTRL, &control);	     \
		CHECK_ERROR(ret < 0, -1, "Unable to get the " #ctrl " on %s "\
		"channel: VIDIOC_G_CTRL failed (id = 0x%x): %s",	     \
		chan2str(ch), control.id, strerror(errno));		     \
	\
		TRACE2(#ctrl " = %i\n", control.value);\
		*value = control.value;\
	\
	} \
	return 0;\
}
#define DECLARE_CTRL(ctrl, ctrl_id, size_type) \
	DECLARE_SET(ctrl, ctrl_id, size_type); \
	DECLARE_GET(ctrl, ctrl_id, size_type);

DECLARE_CTRL(brightness,        V4L2_CID_BRIGHTNESS,             int16_t);
DECLARE_CTRL(contrast,          V4L2_CID_CONTRAST,               uint16_t);
DECLARE_CTRL(hue,               V4L2_CID_HUE,                    int16_t);
DECLARE_CTRL(saturation,        V4L2_CID_SATURATION,             uint16_t);
DECLARE_CTRL(gain,              V4L2_CID_GAIN,                   uint16_t);
DECLARE_CTRL(zoom,              V4L2_CID_DIGITIAL_MULTIPLIER,    uint16_t);
DECLARE_CTRL(gamma,             V4L2_CID_GAMMA,                  uint16_t);
DECLARE_CTRL(sharpness,         V4L2_CID_SHARPNESS,              uint16_t);
DECLARE_CTRL(goplen,            V4L2_CID_XU_GOP_LENGTH,          uint32_t);
DECLARE_CTRL(profile,           V4L2_CID_XU_AVC_PROFILE,         video_profile_t);
DECLARE_CTRL(max_framesize,     V4L2_CID_XU_MAX_FRAME_SIZE,      uint32_t);
DECLARE_CTRL(maxnal,            V4L2_CID_XU_MAX_NAL,             uint32_t);
DECLARE_CTRL(flip_vertical,     V4L2_CID_PU_XU_VFLIP,            video_flip_t);
DECLARE_CTRL(flip_horizontal,   V4L2_CID_PU_XU_HFLIP,            video_flip_t);
DECLARE_CTRL(max_analog_gain,   V4L2_CID_PU_XU_MAX_ANALOG_GAIN,  uint32_t);
DECLARE_CTRL(histogram_eq,      V4L2_CID_PU_XU_HISTO_EQ,         histo_eq_t);
DECLARE_CTRL(sharpen_filter,    V4L2_CID_PU_XU_SHARPEN_FILTER,   uint32_t);
DECLARE_CTRL(gain_multiplier,   V4L2_CID_PU_XU_GAIN_MULTIPLIER,  uint32_t);
DECLARE_CTRL(min_exp_framerate, V4L2_CID_PU_XU_EXP_MIN_FR_RATE,  uint32_t);
DECLARE_CTRL(tf_strength, 	V4L2_CID_PU_XU_TF_STRENGTH,  	 uint32_t);

DECLARE_CTRL(vui, 		MUX_XU_VUI_ENABLE,    uint32_t);
DECLARE_CTRL(compression_quality, MUX_XU_COMPRESSION_Q, uint32_t);
DECLARE_CTRL(avc_level,		MUX_XU_AVC_LEVEL,     uint32_t);
DECLARE_CTRL(pict_timing,	MUX_XU_VUI_ENABLE,	uint32_t);
DECLARE_CTRL(gop_hierarchy_level, MUX_XU_GOP_HIERARCHY_LEVEL, uint32_t);

