/*******************************************************************************
*
* The content of this file or document is CONFIDENTIAL and PROPRIETARY
* to Maxim Integrated Products.  It is subject to the terms of a
* License Agreement between Licensee and Maxim Integrated Products.
* restricting among other things, the use, reproduction, distribution
* and transfer.  Each of the embodiments, including this information and
* any derivative work shall retain this copyright notice.
*
* Copyright (c) 2012 Maxim Integrated Products.
* All rights reserved.
*
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "mxuvc.h"

FILE *fd[NUM_MUX_VID_CHANNELS][NUM_VID_FORMAT];

static void ch_cb(unsigned char *buffer, unsigned int size,
		video_info_t info, void *user_data)
{
	static const char *basename = "out.chX";
	static const char *ext[NUM_VID_FORMAT] = {
			[VID_FORMAT_H264_RAW]    = ".h264",
			[VID_FORMAT_H264_TS]     = ".ts",
			[VID_FORMAT_H264_AAC_TS] = ".ts",
			[VID_FORMAT_MUX]         = ".mux",
			[VID_FORMAT_MJPEG_RAW]   = ".mjpeg",
	};

	video_format_t fmt = (video_format_t) info.format;
	video_channel_t ch = (video_channel_t) user_data;

	if(fmt < FIRST_VID_FORMAT || fmt >= NUM_VID_FORMAT) {
		printf("Unknown Video format\n");
		return;
	}

	if(fd[ch][fmt] == NULL) {
		char *fname = malloc(strlen(basename) + strlen(ext[fmt]) + 1);
		strcpy(fname, basename);
		strcpy(fname + strlen(fname), ext[fmt]);
		fname[6] = ((char) ch + 1) % 10 + 48;
		fd[ch][fmt] = fopen(fname, "w");
	}


	fwrite(buffer, size, 1, fd[ch][fmt]);
	mxuvc_video_cb_buf_done(ch, info.buf_index);
}

void print_format(video_format_t fmt){
	switch(fmt){
	case VID_FORMAT_H264_RAW:
		printf("Format: H264 Elementary\n");
	break;
	case VID_FORMAT_H264_TS:
		printf("Format: H264 TS\n");
	break;
	case VID_FORMAT_MJPEG_RAW:
		printf("Format: MJPEG\n");
	break;
	case VID_FORMAT_YUY2_RAW:
		printf("Format: YUY2\n");
	break;
	case VID_FORMAT_NV12_RAW:
		printf("Format: NV12\n");
	break;
	case VID_FORMAT_H264_AAC_TS:
		printf("Format: H264 AAC TS\n");
	break;
	case VID_FORMAT_MUX:
		printf("Format: MUX\n");
	break;	
	default:
		printf("unsupported format\n");
	}
}

static void close_fds() {
	int i, j;
	for(i=0; i<NUM_MUX_VID_CHANNELS; i++) {
		for(j=0; j<NUM_VID_FORMAT; j++) {
			if(fd[i][j])
				fclose(fd[i][j]);
		}
	}
}

int main(int argc, char **argv)
{
	int ret=0;
	int count=0;
	video_format_t fmt;
	int ch_count = 0, channel;
	uint16_t width, hight;
	int framerate = 0;
	int goplen = 0;
	int value, comp_q;
	noise_filter_mode_t sel;
	white_balance_mode_t bal;
	pwr_line_freq_mode_t freq;
	zone_wb_set_t wb;

	/* Initialize camera */
	ret = mxuvc_video_init("v4l2","dev_offset=0");

	if(ret < 0)
		return 1;

	/* Register callback functions */
	ret = mxuvc_video_register_cb(CH1, ch_cb, (void*)CH1);
	if(ret < 0)
		goto error;
	
	printf("\n");
	//get channel count of MUX channel
	ret = mxuvc_video_get_channel_count(&ch_count);
	printf("Total Channel count: %d\n",ch_count);
	//remove raw channel from count
	ch_count = ch_count - 1;

	for(channel=CH2 ; channel<ch_count; channel++)
	{
		ret = mxuvc_video_register_cb(channel, ch_cb, (void*)channel);
		if(ret < 0)
			goto error;
	}

	ret = mxuvc_video_get_nf(CH1, &sel, (uint16_t *)&value);
	if(ret < 0)
		goto error;
	ret = mxuvc_video_get_nf(CH1, &sel, (uint16_t *)&value);
	if(ret < 0)
		goto error;	
	ret = mxuvc_video_get_wb(CH1, &bal, (uint16_t *)&value);
	if(ret < 0)
		goto error;	
	ret = mxuvc_video_get_pwr_line_freq(CH1, &freq);
	if(ret < 0)
		goto error;	
	value = 0;
	ret = mxuvc_video_get_zone_wb(CH1, &wb, (uint16_t *)&value);
	if(ret < 0)
		goto error;
	printf("wb %d %d\n",wb,(uint16_t)value);
	/*** enquire every channel capabilities ****/
	for(channel=CH1; channel<ch_count ; channel++)
	{
		printf("\nCH%d Channel Config:\n",channel+1);
		ret = mxuvc_video_get_format(channel, &fmt);
		if(ret < 0)
			goto error;
		print_format(fmt);
		ret = mxuvc_video_get_resolution(channel, (uint16_t *)&width, (uint16_t *)&hight);
		if(ret < 0)
			goto error;
		printf("width %d hight %d\n",width,hight);
		mxuvc_video_get_framerate(channel, &framerate);
		printf("framerate %dfps\n",framerate);
		
		if(fmt == VID_FORMAT_H264_RAW || 
				fmt == VID_FORMAT_H264_TS)
		{
			ret = mxuvc_video_get_goplen(channel, &goplen);
			if(ret < 0)
				goto error;
			printf("gop length %d\n",goplen);	
			printf("setting gop length to 30\n");
			ret = mxuvc_video_set_goplen(channel, 30);
			if(ret < 0)
				goto error;
			ret = mxuvc_video_get_goplen(channel, &goplen);
			if(ret < 0)
				goto error;
			printf("gop length %d\n",goplen);
			uint32_t data;
			ret = mxuvc_video_get_max_framesize(channel, &data);
			if(ret < 0)
				goto error;
			printf("max_framesize %d\n",data);
			ret = mxuvc_video_get_vui(channel, &value);
			if(ret < 0)
				goto error;
			ret = mxuvc_video_get_pict_timing(channel, &value);
			if(ret < 0)
				goto error;
			printf("pict timing %d\n",value);
			ret = mxuvc_video_get_gop_hierarchy_level(channel, &value);
			if(ret < 0)
				goto error;
			printf("gop hierarchy level %d\n",value);
		}
	
		if(fmt == VID_FORMAT_MJPEG_RAW){
			ret = mxuvc_video_get_compression_quality(channel, &comp_q);
			if(ret < 0)
				goto error;
			printf("comp quality %d\n",comp_q);
			ret = mxuvc_video_set_compression_quality(channel, 100);
		}
	}

	printf("\n");
	video_profile_t profile;
	ret = mxuvc_video_get_profile(CH1, &profile);
	if(ret < 0)
		goto error;
	printf("CH1 profile %d\n",profile);
	ret = mxuvc_video_set_profile(CH1, PROFILE_BASELINE);
	if(ret < 0)
		goto error;
	printf("changed profile to PROFILE_BASELINE\n");
	ret = mxuvc_video_get_profile(CH1, &profile);
	if(ret < 0)
		goto error;
	printf("CH1 profile %d\n",profile);


	printf("\n");
	for(channel=CH1; channel<ch_count ; channel++){
		/* Start streaming */
		ret = mxuvc_video_start(channel);
		if(ret < 0)
			goto error;
	}

	usleep(5000);

	/* Main 'loop' */
	int counter;
	if (argc > 1){
		counter = atoi(argv[1]);
	} else
		counter = 15;

	while(counter--) {
		if(!mxuvc_video_alive()) {
			goto error;
		}
		printf("\r%i secs left\n", counter+1);

		if (counter == 10){
			mxuvc_video_set_framerate(CH1, 5);
			mxuvc_video_set_framerate(CH2, 5);
			mxuvc_video_set_framerate(CH3, 5);
			uint16_t zoom=0;
			mxuvc_video_get_zoom(CH1, &zoom);
			printf("CH1 zoom = %d\n",zoom);
			mxuvc_video_set_flip_vertical(CH1, 1);
			//mxuvc_video_set_flip_horizontal(CH1, 1);
		}

		if (counter == 6){
			mxuvc_video_set_framerate(CH1, 15);
			mxuvc_video_set_framerate(CH2, 15);
			mxuvc_video_set_framerate(CH3, 15);
			mxuvc_video_set_zoom(CH1, 20);
			mxuvc_video_set_pantilt(CH1, 3600*100, 3600*100);
			mxuvc_video_set_flip_vertical(CH1, 0);
			//mxuvc_video_set_flip_horizontal(CH1, 0);
		}

		if (counter == 5){
			mxuvc_video_set_framerate(CH1, 30);
			mxuvc_video_set_framerate(CH2, 30);
			mxuvc_video_set_framerate(CH3, 30);
			mxuvc_video_set_zoom(CH1, 80);
			mxuvc_video_set_pantilt(CH1, 0, 0);
			mxuvc_video_set_flip_vertical(CH1, 1);
			//mxuvc_video_set_resolution(CH1, 640, 480);
			//mxuvc_video_set_format(CH1, VID_FORMAT_MJPEG_RAW);
			//mxuvc_video_set_flip_horizontal(CH1, 1);
			//mxuvc_video_set_wb(CH1, WB_MODE_MANUAL, 7000);
			//mxuvc_video_set_brightness(CH1, 200);
			//mxuvc_video_set_contrast(CH1, 200);
			//mxuvc_video_set_zoom(CH1, 50);
			//mxuvc_video_set_tf_strength(CH1, 2);
		}

		fflush(stdout);
		sleep(1);
	}

	for(channel=CH1; channel<ch_count ; channel++)
	{	
		/* Stop streaming */
		ret = mxuvc_video_stop(channel);
		if(ret < 0)
			goto error;
	}
	/* Deinitialize and exit */
	mxuvc_video_deinit();

	close_fds();

	printf("Success\n");

	return 0;
error:
	mxuvc_video_deinit();
	close_fds();
	printf("Failure\n");
	return 1;
}
