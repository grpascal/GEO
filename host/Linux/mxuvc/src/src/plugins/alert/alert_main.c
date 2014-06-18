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

struct libusb_device_handle *camera = NULL;
struct libusb_context *ctxt = NULL;
struct libusb_interface *intf = NULL;

pthread_mutex_t count_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  condition_var   = PTHREAD_COND_INITIALIZER;

int disable_alarm = 0;
int kill_thread =0;
pthread_t alarm_thread_id = 0;

//state machine
typedef enum 
{
	STOP = 0,
	RUNNING,
	UNDEFINED
}ALERT_THRD_STATE;

typedef struct {
	unsigned int audioThresholdDB;		
}audio_alert_setting;

typedef struct {
	int reg_id;	/* region number; used when alert is generated */
	int x_offt;	/* starting x offset */
	int y_offt;	/* starting y offset */
	int width;	/* width of region */
	int height;	/* height of region */
	int sensitivity;	/* threshold for motion detection for a macroblock */
	int trigger_percentage;
}motion_alert_setting;

typedef struct {
	alert_type  type;
	alert_action  action;
	audio_alert_setting audio;
	motion_alert_setting motion;	
}alert_setting;

typedef struct {
	alert_type  type;
	audio_alert_data aalert;
	alert_motion_data malert;	
}alert_info;

static int32_t alert_thread_state = STOP;

//alert count
static int alert_state = 0;

//local functions
static void *mxuvc_alert_thread();
static int start_alert_thread(void);

/* initialize alert plugin */
int mxuvc_alert_init(void)
{
	struct libusb_device *camera_dev;
	int ret=0, count;
	struct libusb_config_descriptor *conf_desc;

	TRACE("alert plugin init\n");

	alarm_thread_id = 0;
	kill_thread = 0;	
	disable_alarm = 0;

	if (camera==NULL){	
		ret = init_libusb(&ctxt);
		//ret = libusb_init(&ctxt);
		if (ret) {
			TRACE("libusb_init failed\n");
			return -1;
		}
		camera = libusb_open_device_with_vid_pid(ctxt, 0x0b6a, 0x4d52);
		if (camera == NULL) {
			TRACE("Opening camera failed\n");
			return -1;
		}
		camera_dev = libusb_get_device(camera);
		if (camera_dev == NULL) {
			TRACE("Cannot get device from handle\n");
			return -1;
		}

		ret = libusb_get_active_config_descriptor(camera_dev, &conf_desc);
		if (ret < 0) {
			TRACE("Cannot retrive active config descriptor\n");
			return -1;
		}
		//array = conf_desc->interface;
		for (count = 0; count < conf_desc->bNumInterfaces; count++) {
			intf = (conf_desc->interface + count);
			/* FIXME TODO not the best check; but sufficient for now */
			if (intf->altsetting->bInterfaceClass == 
						LIBUSB_CLASS_VENDOR_SPEC)
				break;
		}
		if (count < conf_desc->bNumInterfaces) {
			ret = libusb_claim_interface(camera, 
						intf->altsetting->bInterfaceNumber);
			if (ret < 0) {
				TRACE("Cannot claim interface %d\n", 
						intf->altsetting->bInterfaceNumber);
				return -1;
			}
			ret = libusb_set_interface_alt_setting(camera, 
					intf->altsetting->bInterfaceNumber, 1);
			if (ret < 0) {
				TRACE("Cannot set interface\n");
				return -1;
			}
		} else {
			TRACE("Cannot find a suitable interface\n");
			return -1;
		}
	} else
		return -1;

	return 0;	
}

int mxuvc_alert_deinit(void)
{
	int ret = 0;
	TRACE("alert plugin deinit\n");
	alert_thread_state = STOP;
	disable_alarm = 1;

	CHECK_ERROR(alert_state != 0, -1,
			"Unable to deinit alert module alert_state is not zero");

	if(intf && camera)
		ret = libusb_set_interface_alt_setting(camera, 
				intf->altsetting->bInterfaceNumber,
				0);
	
	if(alarm_thread_id){
		pthread_mutex_lock( &count_mutex );
		pthread_cond_signal( &condition_var );
		pthread_mutex_unlock( &count_mutex );

		(void) pthread_join(alarm_thread_id, NULL);
	}	

	if(ctxt && camera){
		libusb_release_interface (camera, 
					intf->altsetting->bInterfaceNumber);
		libusb_close (camera);
		exit_libusb(&ctxt);

		ctxt = NULL;
		intf = NULL;
		camera = NULL;
	}
		
	return ret;
}

/* this function sends all the alert related control events to host */
int set_alert(alert_type type,
		alert_action	action,
		unsigned int audioThresholdDB,
		int reg_id,	/* region number; used when alert is generated */
		int x_offt,	/* starting x offset */
		int y_offt,	/* starting y offset */
		int width,	/* width of region */
		int height,	/* height of region */
		int sensitivity,	/* threshold for motion detection for a macroblock */
		int trigger_percentage	/* % of macroblocks to be in motion for alert to be generated */
)
{
	alert_setting setting;
	int ret = 0;
	CHECK_ERROR(camera == NULL, -1, "Alert module is not enabled");

	if(type.b.audio_alert == 1){
		setting.type.b.audio_alert = 1;
		setting.type.b.motion_alert = 0;
		setting.action = action;
		setting.audio.audioThresholdDB = audioThresholdDB;
		TRACE("Setting Audio Alert\n");
	}else if(type.b.motion_alert == 1){
		setting.type.b.audio_alert = 0;
		setting.type.b.motion_alert = 1;
		setting.action = action;

		setting.motion.reg_id = reg_id;
		setting.motion.x_offt = x_offt;
		setting.motion.y_offt = y_offt;
		setting.motion.width = width;
		setting.motion.height = height;
		setting.motion.sensitivity = sensitivity;
		setting.motion.trigger_percentage = trigger_percentage;		
		TRACE("Setting Motion Alert\n");
	}else{
		TRACE("ERROR: Unsupported Alert type");
		return -1;
	}

	if(camera){
		//enable/disable alert
		ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ 0x29, //AV_ALARM
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)&setting,
				/* wLength       */ sizeof(alert_setting),
				/* timeout*/   0 
				);
		CHECK_ERROR(ret < 0, -1, "setting alert failed");
	}

	if((alert_state == 0) && (action == ALARM_ACTION_ENABLE)){
		//start the alert thread
		ret = start_alert_thread();
		CHECK_ERROR(ret < 0, -1, "alert_thread_state failed");
		sleep(1);
		pthread_mutex_lock( &count_mutex );
		pthread_cond_signal( &condition_var );
		pthread_mutex_unlock( &count_mutex );	
	}
	
	if(action == ALARM_ACTION_ENABLE)	
		alert_state = 1;
	else if(action == ALARM_ACTION_DISABLE)
		alert_state = 0;

	return 0;
}
	
int start_alert_thread(void)
{
	int ret;

	if(camera && intf && (alert_thread_state == STOP))
	{
		alert_thread_state = RUNNING;
		ret = pthread_create( &alarm_thread_id, NULL, 
			&mxuvc_alert_thread, NULL);
	
		if(ret){
			TRACE("%s:pthread_create failed, ret = %d",__func__,ret);			
			alert_thread_state = STOP;
			return -1;
		}
	}
	
	return 0;
}

int check_alert_state(void)
{
	if(camera && intf)
		return 1; //enabled

	return 0;	
}

void *mxuvc_alert_thread()
{
	int rx_size;
	alert_info alert;
	int ret;

	while(alert_thread_state == RUNNING) {
		//wait for alarm enable
		pthread_mutex_lock( &count_mutex );
		pthread_cond_wait( &condition_var, &count_mutex );
		pthread_mutex_unlock( &count_mutex );

		TRACE("Alert Enabled or Exiting...\n");

		while(alert_thread_state == RUNNING){
			rx_size = 0;
			memset(&alert, 0, sizeof(alert_info));
			ret = libusb_interrupt_transfer(camera,
							0x86,
							(unsigned char *)&alert,
							sizeof(alert_info),
							&rx_size, 0);
			 
			if ((ret < 0) && (!disable_alarm)) {
				TRACE("ERROR: Interrupt transfer failed %d\n", ret);
				continue;
			}
			if ((rx_size > 0) && (!disable_alarm)){
				if(alert.type.b.audio_alert){
					proces_audio_alert(&alert.aalert);
				}
				if(alert.type.b.motion_alert){
					proces_motion_alert(&alert.malert);
				}	
			}
		}
	}
	
	return NULL;

}

int get_audio_intensity(uint32_t *audioIntensityDB)
{
	int ret=0;

	if(camera){
		ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ 0x2A, //A_INTENSITY
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)audioIntensityDB,
				/* wLength       */ sizeof(int),
				/* timeout*/   0 
				);
			if (ret < 0) {
				TRACE("ERROR: Audio Intensity request failed\n");
				return -1;
			}
	} else {
		TRACE("%s:ERROR-> Alert module is not enabled",__func__);
		return -1;
	}

	return 0;
}
