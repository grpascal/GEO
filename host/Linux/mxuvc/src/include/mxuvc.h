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

#ifndef __MXUVC_H__
#define __MXUVC_H__

#include <stdint.h>

#define MXUVC_TRACE_LEVEL 1

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	FIRST_VID_FORMAT       = 0,
	VID_FORMAT_H264_RAW    = 0,
	VID_FORMAT_H264_TS     = 1,
	VID_FORMAT_MJPEG_RAW   = 2,
	VID_FORMAT_YUY2_RAW    = 3,
	VID_FORMAT_NV12_RAW    = 4,
	VID_FORMAT_GREY_RAW    = 5,
	VID_FORMAT_H264_AAC_TS = 6,
	VID_FORMAT_MUX         = 7,
	NUM_VID_FORMAT
} video_format_t;

typedef enum channel {
	FIRST_VID_CHANNEL = 0,

	/* channels for skype */
	CH_MAIN    = 0,
	CH_PREVIEW = 1,
	NUM_SKYPE_VID_CHANNELS,
	NUM_VID_CHANNEL = NUM_SKYPE_VID_CHANNELS,

	/* channels for ip camera */
	CH1  = 0,
	CH2  = 1,
	CH3  = 2,
	CH4  = 3,
	CH5  = 4,
	CH6  = 5,
	CH7  = 6,
	NUM_MUX_VID_CHANNELS  = CH7+1,
	CH_RAW            = NUM_MUX_VID_CHANNELS,
	NUM_IPCAM_VID_CHANNELS
} video_channel_t;

typedef enum {
	AUD_FORMAT_PCM_RAW  = 0,
	AUD_FORMAT_AAC_RAW  = 1,
	NUM_AUD_FORMAT
} audio_format_t;

typedef enum {
	AUD_CH1 = 0,
	AUD_CH2,
	NUM_AUDIO_CHANNELS
} audio_channel_t;

typedef enum {
	PROFILE_BASELINE = 0,
	PROFILE_MAIN     = 1,
	PROFILE_HIGH     = 2,
	NUM_PROFILE
} video_profile_t;

typedef enum {
	FLIP_OFF 	= 0,
	FLIP_ON  	= 1,
	NUM_FLIP
} video_flip_t;

typedef enum {
	WDR_AUTO 	= 0,
	WDR_MANUAL 	= 1,
	NUM_WDR
} wdr_mode_t;

typedef enum {
	EXP_AUTO = 0,
	EXP_MANUAL  = 1,
	NUM_EXP
} exp_set_t;

typedef enum {
	ZONE_EXP_DISABLE = 0,
	ZONE_EXP_ENABLE  = 1,
	NUM_ZONE_EXP
} zone_exp_set_t;

typedef enum {
	HISTO_EQ_OFF = 0,
	HISTO_EQ_ON =  1,
	NUM_HISTO_EQ
} histo_eq_t;

typedef enum {
    AUDIO_CODEC_TYPE_AAC = 0x1,
    AUDIO_CODEC_TYPE_QAC = 0x1,
    AUDIO_CODEC_TYPE_QPCM = 0x3,
    AUDIO_CODEC_TYPE_Q711 = 0x9,
    AUDIO_CODEC_TYPE_Q722 = 0xa,
    AUDIO_CODEC_TYPE_Q726 = 0xb,
    AUDIO_CODEC_TYPE_Q728 = 0xc,
    AUDIO_CODEC_TYPE_AMRNB = 0x10,
} audio_codec_type_t;

typedef struct
{
    long long timestamp;
    int framesize;
    int samplefreq;
    int channelno;
    int audioobjtype;
    unsigned char *dataptr;
    audio_codec_type_t audiocodectype;
} audio_params_t;

typedef struct 
{
	video_format_t  format;
	uint16_t	width;
	uint16_t	height;
	uint32_t 	framerate;
	uint32_t	goplen;	
	video_profile_t profile;
	uint32_t	bitrate;
	uint32_t	compression_quality;
} video_channel_info_t;

typedef struct {
	uint16_t enable;
	uint16_t width;
	uint16_t height;
	uint16_t x;
	uint16_t y;
} crop_info_t;

typedef struct {
	uint8_t *buf;
	int size;
} motion_stat_t;

typedef struct {
	video_format_t	format;
	uint64_t	ts;
	motion_stat_t	stats;
	int		buf_index;
}__attribute__((packed))video_info_t;

typedef enum {
    	NF_MODE_AUTO = 0,
    	NF_MODE_MANUAL = 1,
   	 NUM_NF
} noise_filter_mode_t;

typedef enum {
	WB_MODE_AUTO = 0,
	WB_MODE_MANUAL = 1,
    	NUM_WB
}white_balance_mode_t;

typedef enum{
	ZONE_WB_DISABLE = 0,
	ZONE_WB_ENABLE	= 1,
	NUM_ZONE_WB,		
}zone_wb_set_t;

typedef enum {
    	PWR_LINE_FREQ_MODE_DISABLE = 0,
    	PWR_LINE_FREQ_MODE_50HZ = 1,
    	PWR_LINE_FREQ_MODE_60HZ = 2
}pwr_line_freq_mode_t;

typedef void (*mxuvc_video_cb_t)(unsigned char *buffer, unsigned int size,
		video_info_t info, void *user_data);

/**************************
 * Video public interface *
 **************************/
int mxuvc_video_init(const char *backend, const char *options);
int mxuvc_video_deinit();
int mxuvc_video_alive();
int mxuvc_video_start(video_channel_t ch);
int mxuvc_video_stop(video_channel_t ch);
int mxuvc_video_register_cb(video_channel_t ch, mxuvc_video_cb_t func,
		void *user_data);
int mxuvc_video_cb_buf_done(video_channel_t ch, int buf_index);
int mxuvc_video_get_channel_count(uint32_t *count);
int mxuvc_video_set_resolution(video_channel_t ch, uint16_t width, uint16_t height);
int mxuvc_video_get_resolution(video_channel_t ch, uint16_t *width, uint16_t *height);
/* Get supported resolutions for the current video format */
/* Run until return value > 0 */
//int mxuvc_get_supported_resolutions(video_channel_t ch, uint16_t *width,
//											uint16_t *height);

int mxuvc_video_set_format(video_channel_t ch, video_format_t fmt);
int mxuvc_video_get_format(video_channel_t ch, video_format_t *fmt);
int mxuvc_video_force_iframe(video_channel_t ch);

int mxuvc_video_set_framerate(video_channel_t ch, uint32_t framerate);
int mxuvc_video_get_framerate(video_channel_t ch, uint32_t *framerate);

int mxuvc_video_set_brightness(video_channel_t ch, int16_t value);
int mxuvc_video_get_brightness(video_channel_t ch, int16_t *value);

int mxuvc_video_set_contrast(video_channel_t ch, uint16_t value);
int mxuvc_video_get_contrast(video_channel_t ch, uint16_t *value);

int mxuvc_video_set_hue(video_channel_t ch, int16_t value);
int mxuvc_video_get_hue(video_channel_t ch, int16_t *value);

int mxuvc_video_set_saturation(video_channel_t ch, uint16_t value);
int mxuvc_video_get_saturation(video_channel_t ch, uint16_t *value);

int mxuvc_video_set_gain(video_channel_t ch, uint16_t value);
int mxuvc_video_get_gain(video_channel_t ch, uint16_t *value);

int mxuvc_video_set_zoom(video_channel_t ch, uint16_t value);
int mxuvc_video_get_zoom(video_channel_t ch, uint16_t *value);

int mxuvc_video_get_pan(video_channel_t ch, int32_t *pan);
int mxuvc_video_set_pan(video_channel_t ch, int32_t tilt);
int mxuvc_video_get_tilt(video_channel_t ch,int32_t *tilt);
int mxuvc_video_set_tilt(video_channel_t ch, int32_t tilt);
int mxuvc_video_get_pantilt(video_channel_t ch, int32_t *pan, int32_t *tilt);
int mxuvc_video_set_pantilt(video_channel_t ch, int32_t pan, int32_t tilt);

int mxuvc_video_set_gamma(video_channel_t ch, uint16_t value);
int mxuvc_video_get_gamma(video_channel_t ch, uint16_t *value);

int mxuvc_video_set_sharpness(video_channel_t ch, uint16_t value);
int mxuvc_video_get_sharpness(video_channel_t ch, uint16_t *value);

int mxuvc_video_set_bitrate(video_channel_t ch, uint32_t value);
int mxuvc_video_get_bitrate(video_channel_t ch, uint32_t *value);

int mxuvc_video_set_goplen(video_channel_t ch, uint32_t value);
int mxuvc_video_get_goplen(video_channel_t ch, uint32_t *value);

int mxuvc_video_set_profile(video_channel_t ch, video_profile_t profile);
int mxuvc_video_get_profile(video_channel_t ch, video_profile_t *profile);

int mxuvc_video_set_maxnal(video_channel_t ch, uint32_t value);
int mxuvc_video_get_maxnal(video_channel_t ch, uint32_t *value);

int mxuvc_video_set_flip_vertical(video_channel_t ch, video_flip_t value);
int mxuvc_video_get_flip_vertical(video_channel_t ch, video_flip_t *value);

int mxuvc_video_set_flip_horizontal(video_channel_t ch, video_flip_t value);
int mxuvc_video_get_flip_horizontal(video_channel_t ch, video_flip_t *value);

int mxuvc_video_set_wdr(video_channel_t ch, wdr_mode_t mode, uint8_t value);
int mxuvc_video_get_wdr(video_channel_t ch, wdr_mode_t *mode, uint8_t *value);

int mxuvc_video_set_exp(video_channel_t ch, exp_set_t sel, uint16_t value);
int mxuvc_video_get_exp(video_channel_t ch, exp_set_t *sel, uint16_t *value);

int mxuvc_video_set_zone_exp(video_channel_t ch, zone_exp_set_t sel, uint16_t value);
int mxuvc_video_get_zone_exp(video_channel_t ch, zone_exp_set_t *sel, uint16_t *value);

/* Controls the minimun framerate the auto exposure algorithm can go to */
int mxuvc_video_set_min_exp_framerate(video_channel_t ch, uint32_t value);
int mxuvc_video_get_min_exp_framerate(video_channel_t ch, uint32_t *value);

int mxuvc_video_set_max_framesize(video_channel_t ch, uint32_t value);
int mxuvc_video_get_max_framesize(video_channel_t ch, uint32_t *value);

/* Controls the maximum sensor analog gain in auto exposure algorithm.
 * Ranges from 0 to 15.*/
int mxuvc_video_set_max_analog_gain(video_channel_t ch, uint32_t value);
int mxuvc_video_get_max_analog_gain(video_channel_t ch, uint32_t *value);

/*  Disables/enables histogram equalization, which creates more contrast
 *  to the image.
 *  0 to disable, 1 to enable */
int mxuvc_video_set_histogram_eq(video_channel_t ch, histo_eq_t value);
int mxuvc_video_get_histogram_eq(video_channel_t ch, histo_eq_t *value);

/* Set different strengths of the sharpening filter.
 * Ranges from 0 to 2, 2 being the strongest */
int mxuvc_video_set_sharpen_filter(video_channel_t ch, uint32_t value);
int mxuvc_video_get_sharpen_filter(video_channel_t ch, uint32_t *value);

/* Controls the auto exposure algorithm to adjust the sensor analog gain and
 * exposure based on different lighting conditions.
 * Ranges from 0 to 256. */
int mxuvc_video_set_gain_multiplier(video_channel_t ch, uint32_t value);
int mxuvc_video_get_gain_multiplier(video_channel_t ch, uint32_t *value);

int mxuvc_video_set_crop(video_channel_t ch, crop_info_t *info);
int mxuvc_video_get_crop(video_channel_t ch, crop_info_t *info);

int mxuvc_video_set_vui(video_channel_t ch, uint32_t value);
int mxuvc_video_get_vui(video_channel_t ch, uint32_t *value);
int mxuvc_video_set_compression_quality(video_channel_t ch, uint32_t value);
int mxuvc_video_get_compression_quality(video_channel_t ch, uint32_t *value);

int mxuvc_video_set_avc_level(video_channel_t ch, uint32_t value);
int mxuvc_video_get_avc_level(video_channel_t ch, uint32_t *value);

int mxuvc_video_get_channel_info(video_channel_t ch, video_channel_info_t *info);

int mxuvc_video_get_pict_timing(video_channel_t ch, uint32_t *value);
int mxuvc_video_set_pict_timing(video_channel_t ch, uint32_t value);

int mxuvc_video_get_gop_hierarchy_level(video_channel_t ch, uint32_t *value);
int mxuvc_video_set_gop_hierarchy_level(video_channel_t ch, uint32_t value);

int mxuvc_video_set_nf(video_channel_t ch, noise_filter_mode_t sel, uint16_t value);
int mxuvc_video_get_nf(video_channel_t ch, noise_filter_mode_t *sel, uint16_t *value);

int mxuvc_video_set_wb(video_channel_t ch, white_balance_mode_t sel, uint16_t value);
int mxuvc_video_get_wb(video_channel_t ch, white_balance_mode_t *sel, uint16_t *value);

int mxuvc_video_set_zone_wb(video_channel_t ch, zone_wb_set_t sel, uint16_t value);
int mxuvc_video_get_zone_wb(video_channel_t ch, zone_wb_set_t *sel, uint16_t *value);

int mxuvc_video_set_pwr_line_freq(video_channel_t ch,pwr_line_freq_mode_t mode);
int mxuvc_video_get_pwr_line_freq(video_channel_t ch,pwr_line_freq_mode_t *mode);

int mxuvc_video_set_tf_strength(video_channel_t ch, uint32_t value);
int mxuvc_video_get_tf_strength(video_channel_t ch, uint32_t *value);
/**************************
 * Audio public interface *
 **************************/
typedef void (*mxuvc_audio_cb_t)(unsigned char *buffer, unsigned int size,
		int format, uint64_t ts, void *user_data, audio_params_t *param);

int mxuvc_audio_init(const char *backend, const char *options);
int mxuvc_audio_deinit();
int mxuvc_audio_alive();
int mxuvc_audio_start(audio_channel_t ch);
int mxuvc_audio_stop(audio_channel_t ch);
int mxuvc_audio_register_cb(audio_channel_t ch, mxuvc_audio_cb_t func, void *user_data);
int mxuvc_audio_set_samplerate(audio_channel_t ch, int samplingFr);
int mxuvc_audio_get_samplerate(audio_channel_t ch);
int mxuvc_audio_set_volume(int volume); /* min=0, max=100 */
int mxuvc_audio_get_volume();
int mxuvc_audio_set_mic_mute(int bMute);  /* mute=1, unmute=0 */
int mxuvc_audio_set_format(audio_channel_t ch, audio_format_t fmt);
int mxuvc_audio_set_vad(uint32_t vad_status);
int mxuvc_audio_get_bitrate(uint32_t *value);
int mxuvc_audio_set_bitrate(uint32_t value);

/* Debug functions */
int mxuvc_debug_startrec(char *outfile);
int mxuvc_debug_stoprec();

/**************************
 * Alert public interface *
 **************************/
/* info passed when motion is detected */
typedef struct {
	int reg_id;		/* region id */
	uint8_t transition;	/* static to motion or vice-versa */
	uint16_t mbs_in_motion;	/* macro blocks in motion */
	uint16_t avg_motion;	/* average motion */
	uint32_t PTS_high;	/* PTS = (PTS_high << 32) | PTS_low */
	uint32_t PTS_low;
}alert_motion_data;

/* initialize alert plugin */
int mxuvc_alert_init(void);
/* de-initialize alert plugin */
int mxuvc_alert_deinit(void);

/* enable alert on motion detect */
int mxuvc_alert_motion_enable(
	void (*callback)(alert_motion_data*, void *data), void *data);	/* callback with motion info */
/* disable motion alert */
int mxuvc_alert_motion_disable(void);
/* set a region for motion detection */
int mxuvc_alert_motion_add_region(
	int reg_id,	/* region number; used when alert is generated */
	int x_offt,	/* starting x offset */
	int y_offt,	/* starting y offset */
	int width,	/* width of region */
	int height /* height of region */
);
int mxuvc_alert_motion_sensitivity_and_level(
    int reg_id, /* region number; used when alert is generated */
	int sensitivity,	/* threshold for motion detection for a macroblock */
    int level		 /* % of macroblocks to be in motion for alert to be generated */
);
/* remove a region from motion detection */
int mxuvc_alert_motion_remove_region(int reg_id);

/* To enable audio alarm:
callback: function will be called from audio alarm thread, if it receives any alarm
return: 0- success, else failure */
int mxuvc_alert_audio_enable(void (*callback)(unsigned int audioIntensityDB, void *data)
        , void *data);
/* API to set audio threshold 
audioThresholdDB: audio alarm threshold */
int mxuvc_alert_audio_set_threshold(unsigned int audioThresholdDB);
/* api to disable audio alarm */
int mxuvc_alert_audio_disable(void);
/* api to get current audio intensity */
int mxuvc_get_current_audio_intensity(unsigned int *audioIntensityDB);

/* burnin API's */
int mxuvc_burnin_init(int font_size, char* file_name);
int mxuvc_burnin_deinit(void);
int mxuvc_burnin_add_text(int idx, char *str, int length, uint16_t X, uint16_t Y);
int mxuvc_burnin_remove_text(int idx);
int mxuvc_burnin_show_time(int x, int y, int frame_num_enable);
int mxuvc_burnin_hide_time(void);
int mxuvc_burnin_update_time(int h, int m, int sec);

/**************************
 * Custom Control Plug-in *
 **************************/
int mxuvc_custom_control_init(void);
int mxuvc_custom_control_deinit(void);
int mxuvc_custom_control_set_vad(uint32_t vad_status);
int mxuvc_custom_control_enable_aec(void);
int mxuvc_custom_control_disable_aec(void);
int mxuvc_custom_control_set_audio_codec_samplerate(unsigned int samplerate);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // #ifdef __MXUVC_H__
