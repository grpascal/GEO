/*******************************************************************************
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

#define AUDIO_THRESHOLD_DB_DEFAULT 100

//static variables
static int alert_enable = 0;
static unsigned int audio_threshold_dB = AUDIO_THRESHOLD_DB_DEFAULT;
static void *cb_auser_data;

void (*audio_callback)(unsigned int audioIntensityDB, void *data);

//

int mxuvc_alert_audio_enable(void (*callback)(unsigned int audioIntensityDB, void *data), void *data)
{
	int ret = 0;
	alert_type type;

	float PowVal;
	unsigned int aud_threshold_val;
	CHECK_ERROR(check_alert_state() != 1, -1,
			"alert module is not initialized");
		
	if(alert_enable==0)
		alert_enable = 1;

	type.b.audio_alert = 1;
	type.b.motion_alert = 0;
	//convert DB to Intensity
	PowVal = (float)(audio_threshold_dB/20.0);
	PowVal = pow(10, PowVal);	
	aud_threshold_val = (unsigned int)(PowVal + 0.5);

	ret = set_alert(type, ALARM_ACTION_ENABLE, aud_threshold_val, -1, 0, 0, 0, 0, 0, 0 );
	CHECK_ERROR(ret != 0, -1, "set_alert failed");

	audio_callback = callback;
	cb_auser_data = data;

	return ret;
}

int mxuvc_alert_audio_set_threshold(unsigned int audioThresholdDB)
{
    int ret = 0;
    alert_type type;

    float PowVal;
    unsigned int aud_threshold_val;
    CHECK_ERROR(check_alert_state() != 1, -1,
            "alert module is not initialized");

    audio_threshold_dB = audioThresholdDB;
    if(alert_enable==0)
        return ret;

    type.b.audio_alert = 1;
    type.b.motion_alert = 0;
    //convert DB to Intensity
    PowVal = (float)(audio_threshold_dB/20.0);
    PowVal = pow(10, PowVal);
    aud_threshold_val = (unsigned int)(PowVal + 0.5);

    ret = set_alert(type, ALARM_ACTION_ENABLE, aud_threshold_val, -1, 0, 0, 0, 0, 0, 0 );
    CHECK_ERROR(ret != 0, -1, "set_alert failed");

    return ret;
}

/* api to disable audio alarm */
int mxuvc_alert_audio_disable(void)
{
	alert_type type;
	int ret = 0;

	CHECK_ERROR(alert_enable == 0, -1,
			"Audio alert module is not initialized");

	if(alert_enable){
		alert_enable = 0;
		type.b.audio_alert = 1;
		type.b.motion_alert = 0;

		ret = set_alert(type, ALARM_ACTION_DISABLE, 0, -1, 0, 0, 0, 0, 0, 0 );
		CHECK_ERROR(ret != 0, -1, "set_alert failed");
	}

	return ret;
}

/* api to get current audio intensity */
int mxuvc_get_current_audio_intensity(unsigned int *audioIntensityDB)
{
	int ret = 0; 

	ret = get_audio_intensity(audioIntensityDB);

	CHECK_ERROR(ret != 0, -1,
			"mxuvc_get_current_audio_intensity failed");

	//convert intensity to db
	if(*audioIntensityDB >= 32767)
		return -1;
	*audioIntensityDB = (20 * log10(*audioIntensityDB)) + 0.5;

	return ret;
}


int proces_audio_alert(audio_alert_data *aalert)
{
	uint32_t audioIntensityDB = aalert->audioIntensityDB;

	//convert intensity to db
	if(audioIntensityDB >= 32767)
		return -1;

	audioIntensityDB = (20 * log10(audioIntensityDB)) + 0.5;

	if(alert_enable && audio_callback)
		audio_callback(audioIntensityDB, cb_auser_data);	

	return 0;
}
