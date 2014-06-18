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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <assert.h>

#include "uvcvideo.h"
#include "uvcext.h"

/*
 * Mapping of the resolutions to their index in the extension control.
 * Please list the supported resolution in increasing order.
 */
#define R(w,h) (((w)<<16)|(h))
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
	{"1920x1080", 16, R(1920,1080)},
};

/* {303B461D-BC63-44c3-8230-6741CAEB5D77} */
#define GUID_VIDCAP_EXT {0x1d,0x46,0x3b,0x30,0x63,0xbc,0xc3,0x44,0x82,0x30,0x67,0x41,0xca,0xeb,0x5d,0x77}
/* {6DF18A70-C113-428e-88C5-4AFF0E286AAA} */
#define GUID_VIDENC_EXT {0x70,0x8a,0xf1,0x6d,0x13,0xc1,0x8e,0x42,0x88,0xc5,0x4a,0xff,0x0e,0x28,0x6a,0xaa}
/* {ba2b92d9-26f2-4294-42ae-e4eb4d68dd06} */
#define AVC_XU_GUID {0xd9,0x92,0x2b,0xba,0xf2,0x26,0x94,0x42,0x42,0xae,0xe4,0xeb,0x4d,0x68,0xdd,0x06}
/* {bd5321b4-d635-ca45-b203-4e0149b301bc} */
#define SKYPE_XU_GUID {0xb4,0x21,0x53,0xbd,0x35,0xd6,0x45,0xca,0xb2,0x03,0x4e,0x01,0x49,0xb3,0x01,0xbc}
/* {DF5DCD12-7D5F-4bba-BB6D-4B625ADD5272} */
#define PU_XU_GUID {0x12,0xcd,0x5d,0xdf,0x5f,0x7d,0xba,0x4b,0xbb,0x6d,0x4b,0x62,0x5a,0xdd,0x52,0x72}

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
 
/* Controls in the XU */
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

	AVC_XU_NUM_CTRLS,
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

enum PU_XU_CTRL {
	PU_XU_ANF_ENABLE = 1,
	PU_XU_NF_STRENGTH,
	PU_XU_TF_STRENGTH,
	PU_XU_ADAPTIVE_WDR_ENABLE,
	PU_XU_WDR_STRENGTH,
	PU_XU_AUTO_EXPOSURE,
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
	PU_XU_MVMT_QUERY,
	PU_XU_NUM_CTRLS,
};


#define UVC_GUID_UVC_CAMERA \
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}
#define UVC_GUID_UVC_PROCESSING \
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01}

#define CT_PANTILT_ABSOLUTE_CONTROL     		0x0d
#define PU_DIGITAL_MULTIPLIER_CONTROL   		0x0e

#define V4L2_CID_DIGITIAL_MULTIPLIER			(V4L2_CID_CAMERA_CLASS_BASE+32)

/* FIXME - update uvc driver instead */
static struct uvc_xu_control_mapping mappings[] = {
	{V4L2_CID_PAN_ABSOLUTE, "Pan, Absolute", UVC_GUID_UVC_CAMERA, CT_PANTILT_ABSOLUTE_CONTROL, 32, 0, V4L2_CTRL_TYPE_INTEGER, UVC_CTRL_DATA_TYPE_SIGNED},
	{V4L2_CID_TILT_ABSOLUTE, "Tilt, Absolute", UVC_GUID_UVC_CAMERA, CT_PANTILT_ABSOLUTE_CONTROL, 32, 32, V4L2_CTRL_TYPE_INTEGER, UVC_CTRL_DATA_TYPE_SIGNED},
	{V4L2_CID_DIGITIAL_MULTIPLIER, "Digital Multiplier", UVC_GUID_UVC_PROCESSING, PU_DIGITAL_MULTIPLIER_CONTROL, 16, 0, V4L2_CTRL_TYPE_INTEGER, UVC_CTRL_DATA_TYPE_SIGNED},
};

struct uvc_xu_data {
/*
	__u8	entity[16]	Extension unit GUID
	__u8	index		Control index in the unit's bmControl bit field
	__u8	selector	Control selector
	__u16	size		Control size (in bytes)
	__u32	flags		See below

	__u32	id;		V4L2 control identifier
	__u8	name[32];	V4L2 control name
	__u8	entity[16];	UVC extension unit GUID
	__u8	selector;	UVC control selector
	__u8	size;		V4L2 control size (in bits)
	__u8	offset;		V4L2 control offset (in bits)
	enum v4l2_ctrl_type v4l2_type;	V4L2 control type
	enum uvc_control_data_type data_type;	UVC control data type
*/
	__u8	entity[16];	/* Extension unit GUID */
	__u8	selector;	/* UVC control selector */
	__u8	size;		/* V4L2 control size (in bits) */
	__u8	offset;		/* V4L2 control offset (in bits) */
	__u32	id;		/* V4L2 control identifier */
	__u8	name[32];	/* V4L2 control name */
};

static struct uvc_xu_data xu_data[] = {
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

    {AVC_XU_GUID,     AVC_XU_PROFILE,        32,  0, V4L2_CID_XU_AVC_PROFILE,    "Profile"},
    {AVC_XU_GUID,     AVC_XU_LEVEL,          32,  0, V4L2_CID_XU_AVC_LEVEL,      "Level"},
    {AVC_XU_GUID,     AVC_XU_PICTURE_CODING, 32,  0, V4L2_CID_XU_PICTURE_CODING, "Picture Coding"},
    {AVC_XU_GUID,     AVC_XU_RESOLUTION,     32,  0, V4L2_CID_XU_RESOLUTION2,    "Resolution"},
    {AVC_XU_GUID,     AVC_XU_GOP_STRUCTURE,  32,  0, V4L2_CID_XU_GOP_STRUCTURE,  "GOP Structure"},
    {AVC_XU_GUID,     AVC_XU_GOP_LENGTH,     32,  0, V4L2_CID_XU_GOP_LENGTH,     "GOP Length"},
    {AVC_XU_GUID,     AVC_XU_BITRATE,        32,  0, V4L2_CID_XU_BITRATE,        "Bitrate"},
    {AVC_XU_GUID,     AVC_XU_FORCE_I_FRAME,  32,  0, V4L2_CID_XU_FORCE_I_FRAME,  "Force I Frame"},
    {AVC_XU_GUID,     AVC_XU_MAX_NAL,        32,  0, V4L2_CID_XU_MAX_NAL,        "Max NAL Units"},
    {AVC_XU_GUID,     AVC_XU_VUI_ENABLE,     32,  0, V4L2_CID_XU_VUI_ENABLE,     "VUI Enable"},
    {AVC_XU_GUID,  AVC_XU_PIC_TIMING_ENABLE, 32,  0, V4L2_CID_XU_PIC_TIMING_ENABLE, "Pic Timing Enable"},
    {AVC_XU_GUID,     AVC_XU_AV_MUX_ENABLE,  32,  0, V4L2_CID_XU_AV_MUX_ENABLE,  "AV Mux Enable"},
    {AVC_XU_GUID,     AVC_XU_MAX_FRAME_SIZE, 32,  0, V4L2_CID_XU_MAX_FRAME_SIZE,  "Max Frame Size"},

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

    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATCOMMIT,   8,  0,  V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT,   "Commit - Stream Type"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATCOMMIT,  16,  8,  V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT+1, "Commit - Width"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATCOMMIT,  16, 24,  V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT+2, "Commit - Height"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATCOMMIT,  32, 40,  V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT+3, "Commit - Frame Interval"},
    {SKYPE_XU_GUID, SKYPE_XU_STREAMFORMATCOMMIT,  32, 72,  V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT+4, "Commit - Bitrate"},

    {SKYPE_XU_GUID, SKYPE_XU_BITRATE,            32,  0,  V4L2_CID_SKYPE_XU_BITRATE,            "Bitrate"},
    {SKYPE_XU_GUID, SKYPE_XU_FRAMEINTERVAL,      32,  0,  V4L2_CID_SKYPE_XU_FRAMEINTERVAL,      "Frame Interval"},
    {SKYPE_XU_GUID, SKYPE_XU_GENERATEKEYFRAME,    8,  0,  V4L2_CID_SKYPE_XU_GENERATEKEYFRAME,   "Generate Key Frame"},
    
    {PU_XU_GUID, PU_XU_ANF_ENABLE,          32,  0,  V4L2_CID_PU_XU_ANF_ENABLE,    "Auto Noise Filter"},
    {PU_XU_GUID, PU_XU_NF_STRENGTH,         32,  0,  V4L2_CID_PU_XU_NF_STRENGTH,   "Noise Filter Strength"},
    {PU_XU_GUID, PU_XU_TF_STRENGTH,         32,  0,  V4L2_CID_PU_XU_TF_STRENGTH,   "Temporal Filter Strength"},
    {PU_XU_GUID, PU_XU_ADAPTIVE_WDR_ENABLE, 32,  0,  V4L2_CID_PU_XU_ADAPTIVE_WDR_ENABLE, "Adaptive WDR Enable"},
    {PU_XU_GUID, PU_XU_WDR_STRENGTH,        32,  0,  V4L2_CID_PU_XU_WDR_STRENGTH,   "WDR Strength"},
    {PU_XU_GUID, PU_XU_AUTO_EXPOSURE,       32,  0,  V4L2_CID_PU_XU_AUTO_EXPOSURE,   "Auto Exposure"},
    {PU_XU_GUID, PU_XU_EXPOSURE_TIME,       32,  0,  V4L2_CID_PU_XU_EXPOSURE_TIME,   "Exposure Time"},
    {PU_XU_GUID, PU_XU_AWB_ENABLE,          32,  0,  V4L2_CID_PU_XU_AUTO_WHITE_BAL,  "Auto White Balance"},
    {PU_XU_GUID, PU_XU_WB_TEMPERATURE,      32,  0,  V4L2_CID_PU_XU_WHITE_BAL_TEMP,  "White Balance Temperature"},
    {PU_XU_GUID, PU_XU_VFLIP,               32,  0,  V4L2_CID_PU_XU_VFLIP,           "Vertical Flip"},
    {PU_XU_GUID, PU_XU_HFLIP,               32,  0,  V4L2_CID_PU_XU_HFLIP,           "Horizontal Flip"},
    {PU_XU_GUID, PU_XU_WB_ZONE_SEL_ENABLE,  32,  0,  V4L2_CID_PU_XU_WB_ZONE_SEL_ENABLE, "White Balance Zone Select"},
    {PU_XU_GUID, PU_XU_WB_ZONE_SEL,         32,  0,  V4L2_CID_PU_XU_WB_ZONE_SEL,     "White Balance Zone"},
    {PU_XU_GUID, PU_XU_EXP_ZONE_SEL_ENABLE, 32,  0,  V4L2_CID_PU_XU_EXP_ZONE_SEL_ENABLE, "Exposure Zone Select"},
    {PU_XU_GUID, PU_XU_EXP_ZONE_SEL,        32,  0,  V4L2_CID_PU_XU_EXP_ZONE_SEL,    "Exposure Zone"},
    {PU_XU_GUID, PU_XU_MAX_ANALOG_GAIN,     32,  0,  V4L2_CID_PU_XU_MAX_ANALOG_GAIN, "Max Analog Gain"},
    {PU_XU_GUID, PU_XU_HISTO_EQ,            32,  0,  V4L2_CID_PU_XU_HISTO_EQ,        "Max Analog Gain"},
    {PU_XU_GUID, PU_XU_SHARPEN_FILTER,      32,  0,  V4L2_CID_PU_XU_SHARPEN_FILTER,  "Sharpen Filter Level"},
    {PU_XU_GUID, PU_XU_GAIN_MULTIPLIER,     32,  0,  V4L2_CID_PU_XU_GAIN_MULTIPLIER, "Exposure Gain Multiplier"},
    /* Crop mode */
    {PU_XU_GUID, PU_XU_CROP,                16,  0,  V4L2_CID_PU_XU_CROP_ENABLE    , "Crop Enable"},
    {PU_XU_GUID, PU_XU_CROP,                16,  16, V4L2_CID_PU_XU_CROP_WIDTH     , "Crop Width"},
    {PU_XU_GUID, PU_XU_CROP,                16,  32, V4L2_CID_PU_XU_CROP_HEIGHT    , "Crop Height"},
    {PU_XU_GUID, PU_XU_CROP,                16,  48, V4L2_CID_PU_XU_CROP_X         , "Crop X"},
    {PU_XU_GUID, PU_XU_CROP,                16,  64, V4L2_CID_PU_XU_CROP_Y         , "Crop Y"},
    {PU_XU_GUID, PU_XU_EXP_MIN_FR_RATE,     32,  0,  V4L2_CID_PU_XU_EXP_MIN_FR_RATE, "Minimum Frame Rate"},
    {PU_XU_GUID, PU_XU_MVMT_QUERY,	    32,  0,  V4L2_CID_PU_XU_MVMT_QUERY,      "Query Camera Movememt State"},
};

static void map_xu(int fd, struct uvc_xu_data *data)
{
	int ret;
	struct uvc_xu_control_info info;
	struct uvc_xu_control_mapping mapping;

	memcpy(info.entity, data->entity, sizeof(data->entity));
	info.index = data->selector - 1;
	info.selector = data->selector;
	info.size = data->size/8; /* XXX - Assumes we have no composite control mappings */
	info.flags =
		UVC_CTRL_FLAG_SET_CUR |
		UVC_CTRL_FLAG_GET_RANGE ; /* XXX - Should make part of struct */
	ret = ioctl(fd, UVCIOC_CTRL_ADD, &info);
	if(ret && errno != EEXIST) {
		fprintf(stderr, "UVCIOC_CTRL_ADD failed: %s\n", strerror(errno));
	}

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
		fprintf(stderr, "UVCIOC_CTRL_MAP_OLD failed (id = 0x%x): %s\n",
				data->id,
				strerror(errno));
	} else {
		return;
	}
	/* Try newer mapping ioctl, we get here only if MAP_OLD fails */
	ret = ioctl(fd, UVCIOC_CTRL_MAP, &mapping);
	if(ret && errno != EEXIST && errno != ENOENT) {
		fprintf(stderr, "UVCIOC_CTRL_MAP failed (id = 0x%x): %s\n",
				data->id,
				strerror(errno));
	}
}

void init_ctrl(int fd)
{
	int i;
	for(i = 0; i < sizeof(xu_data)/sizeof(xu_data[0]); i++) {
		map_xu(fd, &xu_data[i]);
	}
	for(i = 0; i < sizeof(mappings)/sizeof(mappings[0]); i++) {
		int ret = ioctl(fd, UVCIOC_CTRL_MAP_OLD, &mappings[i]);
		if(ret && errno != EEXIST && errno != ENOENT) {
			fprintf(stderr, "UVCIOC_CTRL_MAP_OLD failed (id = 0x%x): %s\n",
					mappings[i].id,
					strerror(errno));
		}
	}
}

int set_v4l2_framerate(int fps)
{
	extern int fd;
	int ret;
	struct v4l2_streamparm streamparam;
	struct v4l2_fract tpf;
	memset(&streamparam, 0, sizeof(streamparam));
	streamparam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	streamparam.parm.capture.timeperframe.numerator = 1;
	streamparam.parm.capture.timeperframe.denominator = fps;

	ret = ioctl(fd, VIDIOC_S_PARM, &streamparam);
	if (ret < 0)
		return ret;
	tpf = streamparam.parm.capture.timeperframe;
	return (float)tpf.denominator/tpf.numerator + 0.5;
}

int get_v4l2_framerate()
{
	extern int fd;
	struct v4l2_streamparm streamparam;
	struct v4l2_fract tpf;
	memset(&streamparam, 0, sizeof(streamparam));
	streamparam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(fd, VIDIOC_G_PARM, &streamparam);
	tpf = streamparam.parm.capture.timeperframe;
	return (float)tpf.denominator/tpf.numerator + 0.5;
}

int get_v4l2_ctrl(int id, int *value)
{
	extern int fd;
	struct v4l2_control control;
	int ret;
	control.id = id;
	ret = ioctl(fd, VIDIOC_G_CTRL, &control);
	if(ret < 0) {
		fprintf(stderr, "VIDIOC_G_CTRL failed (id = 0x%x): %s\n",
				id, strerror(errno));
	} else {
		printf("get_v4l2_ctrl 0x%x = %d\n", control.id, control.value);
		*value = control.value;
	}
	return ret;
}

int set_v4l2_ctrl(int id, int value)
{
	extern int fd;
	struct v4l2_control control;
	int ret;
	control.id = id;
	control.value = value;
	printf("set_v4l2_ctrl 0x%x = %d\n", control.id, control.value);
	ret = ioctl(fd, VIDIOC_S_CTRL, &control);
	if(ret < 0) {
		fprintf(stderr, "VIDIOC_S_CTRL failed (id = 0x%x, value = "
				"%i): %s\n", id, value, strerror(errno));
	}
	return ret;
}

extern int fd;
static struct v4l2_control control;
static struct v4l2_queryctrl queryctrl;
static struct v4l2_querymenu querymenu;

static void enumerate_menu(void)
{
        printf ("  Menu items:\n");

        memset (&querymenu, 0, sizeof (querymenu));
        querymenu.id = queryctrl.id;

        for (querymenu.index = queryctrl.minimum;
             querymenu.index <= queryctrl.maximum;
              querymenu.index++) {
                if (0 == ioctl (fd, VIDIOC_QUERYMENU, &querymenu)) {
                        printf ("  %s\n", querymenu.name);
                } else {
                        perror ("VIDIOC_QUERYMENU");
                        exit (EXIT_FAILURE);
                }
        }
}

void list_ctrl(int fd)
{
	int ret;
	memset(&queryctrl, 0, sizeof(queryctrl));
	memset(&control, 0, sizeof(control));
	for (queryctrl.id = V4L2_CID_BASE;
			queryctrl.id < V4L2_CID_LASTP1;
			queryctrl.id++) {
		if (0 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl)) {
			if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				continue;

			control.id = queryctrl.id;
			printf ("Control %s\n", queryctrl.name);

			if (queryctrl.type == V4L2_CTRL_TYPE_MENU)
				enumerate_menu();
			else {
				ret = ioctl(fd, VIDIOC_G_CTRL, &control);
				if(ret < 0) {
					fprintf(stderr, "VIDIOC_G_CTRL failed: %s\n", strerror(errno));
					continue;
				}
				printf("  cur %10u min %10u max %10u def %10u\n", control.value, queryctrl.minimum, queryctrl.maximum, queryctrl.default_value);
			}
		} else {
			if (errno == EINVAL || errno == EIO)
				continue;

			perror("VIDIOC_QUERYCTRL");
			exit(EXIT_FAILURE);
		}
	}
	for(queryctrl.id = V4L2_CID_XU_BASE;
			queryctrl.id <= V4L2_CID_XU_END;
			queryctrl.id++) {
		ret = ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl);
		if(ret < 0) {
			if (errno == EINVAL || errno == EIO)
				continue;
			
			fprintf(stderr, "VIDIOC_QUERYCTRL failed: %s\n", strerror(errno));
			continue;
		}
                if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                        continue;
		control.id = queryctrl.id;
		printf ("Control %s\n", queryctrl.name);
		ret = ioctl(fd, VIDIOC_G_CTRL, &control);
		if(ret < 0) {
			fprintf(stderr, "VIDIOC_G_CTRL failed: %s\n", strerror(errno));
			continue;
		}
		printf("  cur %10u min %10u max %10u def %10u\n", control.value, queryctrl.minimum, queryctrl.maximum, queryctrl.default_value);
	}
	printf("End of control enumeration\n");
}

int get_skype_stream_control(int fd, struct StreamFormat *format, int commit)
{
	int ret, i;
	int id = commit ? V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT : V4L2_CID_SKYPE_XU_STREAMFORMATPROBE;
	struct v4l2_ext_controls ctrls;
	struct v4l2_ext_control ctrl[5];

	memset(&ctrls, 0, sizeof(ctrls));
	ctrls.count = 5;
	ctrls.controls = ctrl;

	memset(ctrl, 0, sizeof(ctrl));
	for(i = 0; i < 5; i++)
		ctrl[i].id = id+i;

	ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
	if(ret < 0) {
		fprintf(stderr, "VIDIOC_G_EXT_CTRLS failed: %s\n", strerror(errno));
	} else {
		format->bStreamType = ctrl[0].value;
		format->wWidth = ctrl[1].value;
		format->wHeight = ctrl[2].value;
		format->dwFrameInterval = ctrl[3].value;
		format->dwBitrate = ctrl[4].value;
	}
	return ret;
}

int set_skype_stream_control(int fd, struct StreamFormat *format, int commit)
{
	int ret, i;
	int id = commit ? V4L2_CID_SKYPE_XU_STREAMFORMATCOMMIT : V4L2_CID_SKYPE_XU_STREAMFORMATPROBE;
	struct v4l2_ext_controls ctrls;
	struct v4l2_ext_control ctrl[5];

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

	ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
	if(ret < 0) {
		fprintf(stderr, "VIDIOC_S_EXT_CTRLS failed: %s\n", strerror(errno));
	}
	return ret;
}

int get_v4l2_crop(struct crop_info *info)
{
	extern int fd;
	int ret, i;
	int id = V4L2_CID_PU_XU_CROP;
	struct v4l2_ext_controls ctrls;
	struct v4l2_ext_control ctrl[5];

	assert(info != NULL);
	memset(info, 0, sizeof(struct crop_info));

	memset(&ctrls, 0, sizeof(ctrls));
	ctrls.count = 5;
	ctrls.controls = ctrl;

	memset(ctrl, 0, sizeof(ctrl));
	for(i = 0; i < 5; i++)
		ctrl[i].id = id+i;

	ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
	if(ret < 0) {
		fprintf(stderr, "VIDIOC_G_EXT_CTRLS failed: %s\n", strerror(errno));
	} else {
		info->enable = ctrl[0].value;
		info->width  = ctrl[1].value;
		info->height = ctrl[2].value;
		info->x      = ctrl[3].value;
		info->y      = ctrl[4].value;
	}
	return ret;
}

int set_v4l2_crop(struct crop_info *info)
{
	extern int fd;
	int ret, i;
	int id = V4L2_CID_PU_XU_CROP;
	struct v4l2_ext_controls ctrls;
	struct v4l2_ext_control ctrl[5];

	memset(&ctrls, 0, sizeof(ctrls));
	ctrls.count = 5;
	ctrls.controls = ctrl;

	memset(ctrl, 0, sizeof(ctrl));
	for(i = 0; i < 5; i++)
		ctrl[i].id = id+i;
	ctrl[0].value =	info->enable;
	ctrl[1].value =	info->width;
	ctrl[2].value =	info->height;
	ctrl[3].value =	info->x;
	ctrl[4].value =	info->y;

	ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
	if(ret < 0) {
		fprintf(stderr, "VIDIOC_S_EXT_CTRLS failed: %s\n", strerror(errno));
	}
	return ret;
}

#if 0
/* XXX - should come from the descriptors */
static const int skype_unit_id = 7;

int get_skype_stream_control(int fd, struct StreamFormat *format, int commit)
{
	int ret;
	struct uvc_xu_control ctrl;

	ctrl.unit = skype_unit_id;
	ctrl.selector = commit ? SKYPE_XU_STREAMFORMATCOMMIT : SKYPE_XU_STREAMFORMATPROBE;
	ctrl.size = sizeof(struct StreamFormat);
	ctrl.data = (void*)format;

	ret = ioctl(fd, UVCIOC_CTRL_GET, &ctrl);
	if(ret < 0) {
		fprintf(stderr, "UVCIOC_CTRL_GET failed: %s\n", strerror(errno));
	}
	return ret;
}

int set_skype_stream_control(int fd, struct StreamFormat *format, int commit)
{
	int ret;
	struct uvc_xu_control ctrl;

	ctrl.unit = skype_unit_id;
	ctrl.selector = commit ? SKYPE_XU_STREAMFORMATCOMMIT : SKYPE_XU_STREAMFORMATPROBE;
	ctrl.size = sizeof(struct StreamFormat);
	ctrl.data = (void*)format;

	ret = ioctl(fd, UVCIOC_CTRL_SET, &ctrl);
	if(ret < 0) {
		fprintf(stderr, "UVCIOC_CTRL_GET failed: %s\n", strerror(errno));
	}
	return ret;
}

int get_skype_stream_control(int fd, struct StreamFormat *format, int commit)
{
	int ret;
	struct uvc_xu_control_query q;

	q.unit = skype_unit_id;
	q.selector = commit ? SKYPE_XU_STREAMFORMATCOMMIT : SKYPE_XU_STREAMFORMATPROBE;
	q.query = UVC_GET_CUR;
	q.size = sizeof(struct StreamFormat);
	q.data = (void*)format;

	ret = ioctl(fd, UVCIOC_CTRL_QUERY, &q);
	if(ret < 0) {
		fprintf(stderr, "UVCIOC_CTRL_QUERY failed: %s\n", strerror(errno));
	}
	return ret;
}

int set_skype_stream_control(int fd, struct StreamFormat *format, int commit)
{
	int ret;
	struct uvc_xu_control_query q;

	q.unit = skype_unit_id;
	q.selector = commit ? SKYPE_XU_STREAMFORMATCOMMIT : SKYPE_XU_STREAMFORMATPROBE;
	q.query = UVC_SET_CUR;
	q.size = sizeof(struct StreamFormat);
	q.data = (void*)format;

	ret = ioctl(fd, UVCIOC_CTRL_QUERY, &q);
	if(ret < 0) {
		fprintf(stderr, "UVCIOC_CTRL_QUERY failed: %s\n", strerror(errno));
	}
	return ret;
}

void dump_skype(int fd)
{
	int i, j, len;
	int ret;
	struct uvc_xu_control_query q;
	__u8 buf[64];

	q.unit = skype_unit_id;
	q.selector = 1;
	q.query = UVC_GET_LEN;
	q.size = 2;
	q.data = buf;

	for(i = 1; i < 33; i++) {
		q.selector = i;
		q.query = UVC_GET_LEN;
		q.size = 2;
		ret = ioctl(fd, UVCIOC_CTRL_QUERY, &q);
		if(ret < 0) {
			fprintf(stderr, "UVCIOC_CTRL_QUERY failed: %s\n", strerror(errno));
			continue;
		}
		len = buf[0]+256*buf[1];
		printf("selector %d len = %u\n", i, len);

		q.query = UVC_GET_INFO;
		q.size = 1;
		ret = ioctl(fd, UVCIOC_CTRL_QUERY, &q);
		if(ret < 0) {
			fprintf(stderr, "UVCIOC_CTRL_QUERY failed: %s\n", strerror(errno));
			continue;
		}
		printf("selector %d info = %u\n", i, buf[0]);

		q.query = UVC_GET_CUR;
		q.size = len;
		ret = ioctl(fd, UVCIOC_CTRL_QUERY, &q);
		if(ret < 0) {
			fprintf(stderr, "UVCIOC_CTRL_QUERY failed: %s\n", strerror(errno));
			continue;
		}
		printf("selector %d cur = ", i);
		for(j = 0; j < len; j++)
		{
			printf("%02x ", buf[j]);
		}
		printf("\n");
	}
}
#endif
