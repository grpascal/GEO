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
#include <string.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include "libusb/handle_events.h"
#include <mxuvc.h>
#include <alert.h>
#include <common.h>
#include <sys/stat.h>
#include <fcntl.h>

static struct libusb_device_handle *camera = NULL;
static struct libusb_context *ctxt = NULL;

#define FWPACKETSIZE 4088

static int usb_send_file(struct libusb_device_handle *dhandle, 
			const char *filename, int fwpactsize,unsigned char brequest)
{
	int r, ret;
	int total_size;
	struct stat stfile;
	FILE *fd; 
	char *buffer;

	if(stat(filename,&stfile))
		return -1;
	if(stfile.st_size <= 0){
		printf("ERR: Invalid file provided\n");
		return -1;
	}

#if !defined(_WIN32)
	fd = fopen(filename, "rb");
#else
	ret = fopen_s(&fd,filename, "rb");
#endif
	
	total_size = stfile.st_size;
	buffer = malloc(fwpactsize);

	printf("Sending file of size %d\n",total_size);

	while(total_size > 0){
		int readl = 0;
		if(fwpactsize > total_size)
			readl = total_size;
		else
			readl = fwpactsize;

		ret = (int)fread(buffer, readl, 1, fd);

		r = libusb_control_transfer(dhandle,
				/* bmRequestType*/
				LIBUSB_ENDPOINT_OUT |LIBUSB_REQUEST_TYPE_VENDOR |
				LIBUSB_RECIPIENT_INTERFACE,
				/* bRequest     */brequest,
				/* wValue       */0,
				/* wIndex       */0,
				/* Data         */
				(unsigned char *)buffer,
				/* wLength       */ readl,
				0); 
		if(r<0){
			printf("ERR: Req 0x%x failed\n",brequest);	
			ret = -1;
			break;
		}
		if(ret == EOF){
			ret = 0;
			break;
		}

		total_size = total_size - readl;
	}

	if(fd)fclose(fd);
	if(buffer)free(buffer);

	return ret;
}

int mxuvc_burnin_init(int font_size, char* file_name)
{
	int ret=0;
	
	if (camera == NULL){
		ret = init_libusb(&ctxt);
		if (ret) {
			TRACE("libusb_init failed\n");
			return -1;
		}
		camera = libusb_open_device_with_vid_pid(ctxt, 0x0b6a, 0x4d52);
		if (camera == NULL) {
			TRACE("Opening camera failed\n");
			return -1;
		}
	}

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ ADD_FONT,
				/* wValue        */ font_size,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ NULL,
				/* wLength       */ 0,
				/* timeout*/   0 
				);
	CHECK_ERROR(ret < 0, -1, "ADD_FONT failed");
	
	ret = usb_send_file(camera, file_name, FWPACKETSIZE, SEND_FONT_FILE);

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ FONT_FILE_DONE,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ NULL,
				/* wLength       */ 0,
				/* timeout*/   0 
				);
	CHECK_ERROR(ret < 0, -1, "FONT_FILE_DONE failed");
	//50ms delay for raptor to load and decompress the font
	usleep(50000);
	return ret;
}

int mxuvc_burnin_deinit(void){
	int ret = 0;

	if(camera){
		libusb_close (camera);
		exit_libusb(&ctxt);

		ctxt = NULL;
		camera = NULL;
	}

	return ret;

}

int mxuvc_burnin_add_text(int idx, char *str, int length, uint16_t X, uint16_t Y )
{
	int ret = 0;
	uint32_t position = 0;
	if(str == NULL || length<= 0){
		printf("ERR: %s str/length is invalid\n",__func__);
		return -1;
	}
	position = (X<<16) + Y;

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ ADD_TEXT,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)&position,
				/* wLength       */ 4,
				/* timeout*/   0 
				);
	CHECK_ERROR(ret < 0, -1, "ADD_TEXT failed");
	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ TEXT,
				/* wValue        */ idx,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)str,
				/* wLength       */ length,
				/* timeout*/   0 
				);

	CHECK_ERROR(ret < 0, -1, "TEXT failed");
	//5ms delay for add text to complete in raptor
	usleep(5000);
	return ret;
}

int mxuvc_burnin_remove_text(int idx)
{
	int ret = 0;

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ REMOVE_TEXT,
				/* wValue        */ idx,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ NULL,
				/* wLength       */ 0,
				/* timeout*/   0 
				);
	//5ms delay for remove text to complete in raptor
	usleep(5000);
	return ret;	
}

int mxuvc_burnin_show_time(int x, int y, int frame_num_enable)
{
	int ret = 0;
	int position = (x<<16) + y;

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ SHOW_TIME,
				/* wValue        */ frame_num_enable,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)&position,
				/* wLength       */ 4,
				/* timeout*/   0 
				);
	//5ms delay for show time to complete in raptor
	usleep(5000);
	return ret;
}

int mxuvc_burnin_hide_time(void)
{
	int ret = 0;

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ HIDE_TIME,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ NULL,
				/* wLength       */ 0,
				/* timeout*/   0 
				);
	//5ms delay for hide time to complete in raptor
	usleep(5000);
	return ret;
}

int mxuvc_burnin_update_time(int h, int m, int sec)
{
	int ret = 0;
	int Time = ((h<<16) | (m<<8)) + sec;

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ UPDATE_TIME,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)&Time,
				/* wLength       */ 4,
				/* timeout*/   0 
				);
	//5ms delay for update time to complete in raptor
	usleep(5000);
	return ret;	
}
