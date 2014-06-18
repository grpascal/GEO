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

FILE *fd;

static void ch_cb(unsigned char *buffer, unsigned int size,
		video_info_t info, void *user_data)
{
	static const char *basename = "out.chX";

	video_format_t fmt = (video_format_t) info.format;
	video_channel_t ch = (video_channel_t) user_data;

	if(fmt < FIRST_VID_FORMAT || fmt >= NUM_VID_FORMAT) {
		printf("Unknown Video format\n");
		return;
	}

	if(fd == NULL) {
		char *fname = malloc(strlen(basename) + strlen(".y8") + 1);
		strcpy(fname, basename);
		strcpy(fname + strlen(fname), ".y8");
		fname[6] = ((char) ch + 1) % 10 + 48;
		fd = fopen(fname, "w");
	}

	fwrite(buffer, size, 1, fd);
	mxuvc_video_cb_buf_done(ch, info.buf_index);
}

static void close_fds() {
	fclose(fd);
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
	int ch;

	/* Initialize camera */
	ret = mxuvc_video_init("v4l2","dev_offset=0");

	if(ret < 0)
		return 1;
	ret = mxuvc_video_get_channel_count(&ch_count);
	printf("Total Channel count: %d\n",ch_count);
	ch = ch_count-1;
	/* Register callback functions */
	ret = mxuvc_video_register_cb(ch_count-1, ch_cb, (void*)ch);
	if(ret < 0)
		goto error;
	
	printf("\n");
	//get channel count of MUX channel
	ret = mxuvc_video_get_channel_count(&ch_count);
	printf("Total Channel count: %d\n",ch_count);

	ret = mxuvc_video_start(ch_count - 1);
	if(ret < 0)
		goto error;

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
		printf("\r%i secs left", counter+1);


		fflush(stdout);
		sleep(1);
	}

	ret = mxuvc_video_stop(ch_count - 1);
	if(ret < 0)
		goto error;
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
