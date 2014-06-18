/*******************************************************************************
*
* The content of this file or document is CONFIDENTIAL and PROPRIETARY
* to GEO Semiconductor.  It is subject to the terms of a License Agreement
* between Licensee and GEO Semiconductor, restricting among other things,
* the use, reproduction, distribution and transfer.  Each of the embodiments,
* including this information and any derivative work shall retain this
* copyright notice.
*
* Copyright 2013 GEO Semiconductor, Inc.
* All rights reserved.
*
* QuArc and Mobilygen are registered trademarks of GEO Semiconductor.
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

/* initialize custom control plugin */
int mxuvc_custom_control_init(void)
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

	return ret;
}

int mxuvc_custom_control_deinit(void)
{
	if (camera){
		libusb_close (camera);
		exit_libusb(&ctxt);
		ctxt = NULL;
		camera = NULL;
	}

	return 0;		
}

int mxuvc_custom_control_set_vad(uint32_t vad_status)
{
	int ret = 0;

	if(camera){
		ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ AUDIO_VAD,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)&vad_status,
				/* wLength       */ sizeof(uint32_t),
				/* timeout*/   0 
				);
			if (ret < 0) {
				TRACE("ERROR: Send Vad Status failed\n");
				return -1;
			}
	} else {
		TRACE("%s:ERROR-> Custom Control Plug-in is not enabled",__func__);
		return -1;
	}

	return ret;
}

int mxuvc_custom_control_enable_aec(void)
{
	int ret=0;

	if(camera){
		TRACE("Set AEC Enable\n");
		ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ AEC_ENABLE,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ NULL,
				/* wLength       */ 0,
				/* timeout*/   0 
				);
		if (ret < 0) {
			TRACE("ERROR: AEC Enable failed\n");
			return -1;
		}
	} else {
		TRACE("%s:ERROR-> Custom Control Plug-in is not enabled",__func__);
		return -1;
	}

	return 0;
}

int mxuvc_custom_control_disable_aec(void)
{
	int ret=0;

	if(camera){
		TRACE("Set AEC Disable\n");
		ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ AEC_DISABLE,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ NULL,
				/* wLength       */ 0,
				/* timeout*/   0 
				);
		if (ret < 0) {
			TRACE("ERROR: AEC Disbale failed\n");
			return -1;
		}
	} else {
		TRACE("%s:ERROR-> Custom Control Plug-in is not enabled",__func__);
		return -1;
	}

	return 0;
}

int mxuvc_custom_control_set_audio_codec_samplerate(unsigned int samplerate)
{
	int ret=0;

	if(camera){
		TRACE("Set Audio Codec Samplerate to %d\n",samplerate);
		ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ AEC_SET_SAMPLERATE,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)&samplerate,
				/* wLength       */ sizeof(unsigned int),
				/* timeout*/   0 
				);
		if (ret < 0) {
			TRACE("ERROR: Set AEC samplerate failed\n");
			return -1;
		}
	} else {
		TRACE("%s:ERROR-> Custom Control Plug-in is not enabled",__func__);
		return -1;
	}

	return 0;
}
