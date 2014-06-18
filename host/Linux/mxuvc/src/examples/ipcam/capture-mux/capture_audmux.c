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
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <pthread.h>

#include "mxuvc.h"
FILE *fd;
#define OBJECTTYPE_AACLC (2)
#define ADTS_HEADER_LEN (7)

static const int SampleFreq[12] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000
};

void get_adts_header(audio_params_t *h, unsigned char *adtsHeader)
{
    int i;
    int samplingFreqIndex;

    // clear the adts header
    for (i = 0; i < ADTS_HEADER_LEN; i++)
    {
        adtsHeader[i] = 0;
    }

    // bits 0-11 are sync
    adtsHeader[0] = 0xff;
    adtsHeader[1] = 0xf0;

    // bit 12 is mpeg4 which means clear the bit

    // bits 13-14 is layer, always 00

    // bit 15 is protection absent (no crc)
    adtsHeader[1] |= 0x1;

    // bits 16-17 is profile which is 01 for AAC-LC
    adtsHeader[2] = 0x40;

    // bits 18-21 is sampling frequency index
    for (i=0; i<12; i++)
    {
        if ( SampleFreq[i] == h->samplefreq )
        {
            samplingFreqIndex =  i;
            break;
        }
    }


    adtsHeader[2] |= (samplingFreqIndex << 2);

    // bit 22 is private

    // bit 23-25 is channel config.  However since we are mono or stereo
    // bit 23 is always zero
    adtsHeader[3] = h->channelno << 6;

    // bits 26-27 are original/home and zeroed

    // bits 28-29 are copyright bits and zeroed

    // bits 30-42 is sample length including header len.  First we get qbox length,
    // then sample length and then add header length


    // adjust for headers
    int frameSize = h->framesize + ADTS_HEADER_LEN;

    // get upper 2 bits of 13 bit length and move them to lower 2 bits
    adtsHeader[3] |= (frameSize & 0x1800) >> 11;

    // get middle 8 bits of length
    adtsHeader[4] = (frameSize & 0x7f8) >> 3;

    // get lower 3 bits of length and put as 3 msb
    adtsHeader[5] = (frameSize & 0x7) << 5;

    // bits 43-53 is buffer fulless but we use vbr so 0x7f
    adtsHeader[5] |= 0x1f;
    adtsHeader[6] = 0xfc;

    // bits 54-55 are # of rdb in frame which is always 1 so we write 1 - 1 = 0
    // which means do not write

}

// default sampling frequency 
int samp_freq = 16000;
#define BOARD_SAMPLE_RATE 16000
uint64_t sam_interval; 

uint64_t prev_ts = 0;

uint64_t permissible_range = 10; //in percentage

uint64_t upper_sam_interval;
uint64_t lower_sam_interval;

static void audio_cb(unsigned char *buffer, unsigned int size,
		int format, uint64_t ts, void *user_data, audio_params_t *param)
{
	audio_format_t fmt = (audio_format_t) format;
	unsigned char adtsHeader[7];

	switch(fmt) {
	case AUD_FORMAT_AAC_RAW: //TBD
		if(fd == NULL){
			fd = fopen("out.audio.aac", "w");
			//calculate required sampling interval
			//(1024/sam_freq)*90  //90 is resampler freq
			sam_interval = (uint64_t)(((float)(1024*1000/samp_freq))*90);
			uint64_t percent = (uint64_t)(sam_interval*permissible_range)/100;
			upper_sam_interval = (uint64_t)(sam_interval + percent);
			lower_sam_interval = (uint64_t)(sam_interval - percent);
			printf("sam_interval %lld upper_limit %lld low_limit %lld\n",
					sam_interval,upper_sam_interval,lower_sam_interval);
		}

		if((((ts-prev_ts) > upper_sam_interval) || ((ts-prev_ts) < lower_sam_interval)) && (prev_ts))
			printf("out of range: %lld, last ts %lld preset ts %lld\n",(ts-prev_ts),prev_ts, ts);

		prev_ts = ts;
			
		if(param->samplefreq != samp_freq)	
			printf("Wrong sampling freq, expected %d hz received pkt with %dhz\n",samp_freq,param->samplefreq);

		get_adts_header(param, adtsHeader);
		fwrite(adtsHeader, ADTS_HEADER_LEN, 1, fd);
		break;
	case AUD_FORMAT_PCM_RAW:
		if(fd == NULL)
			fd = fopen("out.audio.pcm", "w");
		break;
	default:
		printf("Audio format not supported\n");
		return;
	}

	fwrite(buffer, size, 1, fd);
}

static void ch_cb(unsigned char *buffer, unsigned int size,
		video_info_t info, void *user_data)
{
	//dummy to start video
}

static void close_fds() {
	fclose(fd);
}

int main(int argc, char **argv)
{
	int ret;
	int counter=0;
	int bitrate;
	int aec_enable = 0;

	if ( argc < 2 ){
		printf("USAGE: capture_audmux <aec_enable 0/1>	[sampling freq 8000|16000|24000] [no:of seconds to run the test]\n");
		return 0;
	}

	ret = mxuvc_video_init("v4l2","dev_offset=0");
	if(ret < 0)
		return 1;
	/* Register callback functions */
	ret = mxuvc_video_register_cb(CH1, ch_cb, (void*)CH1);
	if(ret < 0)
		goto error;

	ret = mxuvc_audio_init("alsa","device = MAX64380");
	if(ret<0)
		goto error;

	ret = mxuvc_audio_register_cb(AUD_CH2, audio_cb, NULL);
	if(ret < 0)
		goto error;
	
	mxuvc_custom_control_init();
	
	if(argv[1] != NULL)
		aec_enable = atoi(argv[1]);

	if(aec_enable)
	{
		ret = mxuvc_custom_control_enable_aec();
		if(ret < 0)
			goto error;
	}
	else
	{
		ret = mxuvc_custom_control_disable_aec();
		if(ret < 0)
			goto error;
	}

	//TBD
	ret = mxuvc_audio_set_format(AUD_CH2, AUD_FORMAT_AAC_RAW);
	if(ret < 0)
		goto error;

	if(aec_enable){
		ret = mxuvc_custom_control_set_audio_codec_samplerate(BOARD_SAMPLE_RATE);
		if(ret < 0)
			goto error;
	}

	if(argc >  2)
		samp_freq = atoi(argv[2]);

	if(samp_freq == 8000 || samp_freq == 16000 || samp_freq == 24000){		
		ret = mxuvc_audio_set_samplerate(AUD_CH2, samp_freq);
		if(ret < 0)
			goto error;
	}else 
		printf("ERR: Unsupported sampling frequency %d\n",samp_freq);

	mxuvc_audio_set_volume(100);
	if(ret < 0)
		goto error;
	ret = mxuvc_audio_get_bitrate(&bitrate);
	if(ret < 0)
		goto error;
	printf("audio bitrate %d\n",bitrate);
	
	sleep(1);

	/* Main 'loop' */
	if(argc > 3){
		counter = atoi(argv[3]);
	} 
	else{
		counter = 15;
	}
	
	ret = mxuvc_audio_start(AUD_CH2);
	if(ret < 0)
		goto error;

	while(counter--) {
		if (!mxuvc_audio_alive())
			goto error;
		printf("\r%i secs left", counter+1);
		fflush(stdout);
		sleep(1);

		/* uncomment to test Mute/Unmute */
		/*	if (counter >= 15)
			mxuvc_set_mic_mute(1);
		else
			mxuvc_set_mic_mute(0);*/
	}
	
	/* Stop audio streaming */
	ret = mxuvc_audio_stop(AUD_CH2);
	if(ret < 0)
		goto error;


	mxuvc_audio_deinit();
	mxuvc_video_deinit();
	mxuvc_custom_control_deinit();
	close_fds();
	return 0;

error:
	mxuvc_audio_deinit();
	close_fds();	

	printf("Failed\n");
	return 1;
}
