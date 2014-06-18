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

#define MOTION_SENSITIVITY_DEFAULT 255
#define MOTION_LEVEL_DEFAULT 100
#define ROI_MAX     16
#define MIN_SENSITIVITY 16 
#define MAX_SENSITIVITY 200
#define MIN_LEVEL 1
#define MAX_LEVEL 100

//local variables
static int alert_enable = 0;
static void *cb_muser_data;

typedef struct{
        int     x_offset;
        int     y_offset;
        int     width;
        int     height;
        int     sensitivity;
        int     level;
}region_params_t;

static region_params_t region_params[ROI_MAX]= {
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 0 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 1 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 2 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 3 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 4 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 5 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 6 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 7 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 8 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 9 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 10 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 11 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 12 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 13 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 14 */
        {0,0,0,0,MOTION_SENSITIVITY_DEFAULT,MOTION_LEVEL_DEFAULT}, /* 15 */
    };
void (*motion_callback)(alert_motion_data*, void *data);

/* enable alert on motion detect */
int mxuvc_alert_motion_enable(
	void (*callback)(alert_motion_data*, void *data),	/* callback with motion info */
	void *data)
{
	int ret = 0;
	alert_type type;

	CHECK_ERROR(check_alert_state() != 1, -1,
			"alert module is not initialized");
		
	if(alert_enable == 0)
		alert_enable = 1;
	else {
		CHECK_ERROR(alert_enable == 1, -1,
			"Motion alert module is already initialized");
	}
	type.b.audio_alert = 0;
	type.b.motion_alert = 1;

	ret = set_alert(type, ALARM_ACTION_ENABLE, 0, -1, 0, 0, 0, 0, 0, 0 );
	CHECK_ERROR(ret != 0, -1, "set_alert failed");

	motion_callback	= callback;
	cb_muser_data = data;
	
	return ret;
}


/* disable motion alert */
int mxuvc_alert_motion_disable(void)
{
	int ret = 0;
	alert_type type;

	CHECK_ERROR(alert_enable == 0, -1,
		"Motion alert module is not initialized");

	if(alert_enable){
		alert_enable = 0;

		type.b.audio_alert = 0;
		type.b.motion_alert = 1;
		ret = set_alert(type, ALARM_ACTION_DISABLE, 0, -1, 0, 0, 0, 0, 0, 0 );
		
		CHECK_ERROR(ret != 0, -1, "set_alert failed");
	}

	return ret;	
}

/* set a region for motion detection */
int mxuvc_alert_motion_add_region(
	int reg_id,	/* region number; used when alert is generated */
	int x_offt,	/* starting x offset */
	int y_offt,	/* starting y offset */
	int width,	/* width of region */
	int height	/* height of region */
)
{
	int ret = 0;
	alert_type type;

	CHECK_ERROR(alert_enable == 0, -1,
		"Motion alert module is not initialized");
	if(reg_id >= ROI_MAX)
	{
	    ERROR(-1,"Reached Maximum Number of Region of Interest "
	            "that can be added for the motion alert \n");
	}

	type.b.audio_alert = 0;
	type.b.motion_alert = 1;
	region_params[reg_id].x_offset = x_offt;
	region_params[reg_id].y_offset = y_offt;
	region_params[reg_id].width = width;
	region_params[reg_id].height = height;

	ret = set_alert(type, ALARM_ACTION_ADD, 0, reg_id,
	        region_params[reg_id].x_offset,
	        region_params[reg_id].y_offset,
	        region_params[reg_id].width,
	        region_params[reg_id].height,
	        region_params[reg_id].sensitivity,
	        region_params[reg_id].level);
	
	CHECK_ERROR(ret != 0, -1, "set_alert failed");	

	return ret;
}


/* set a sensitivity and level */
int mxuvc_alert_motion_sensitivity_and_level(
    int reg_id, /* region number; used when alert is generated */
    int sensitivity,
    int level
)
{
    int ret = 0;
    alert_type type;

    CHECK_ERROR(alert_enable == 0, -1,
        "Motion alert module is not initialized");

    if(reg_id >= ROI_MAX)
    {
        ERROR(-1,"Reached Maximum Number of Region of Interest "
                "that can be added for the motion alert \n");
    }

    type.b.audio_alert = 0;
    type.b.motion_alert = 1;

    if(sensitivity < MIN_SENSITIVITY)
    {
        sensitivity =  MIN_SENSITIVITY;
    }
    else if(sensitivity > MAX_SENSITIVITY)
    {
        sensitivity =  MAX_SENSITIVITY;
    }

    if(level < MIN_LEVEL)
    {
        level =  MIN_LEVEL;
    }
    else if(level > MAX_LEVEL)
    {
        sensitivity =  MAX_LEVEL;
    }
    region_params[reg_id].sensitivity = sensitivity;
    region_params[reg_id].level = level;

    ret = set_alert(type, ALARM_ACTION_ADD, 0, reg_id,
            region_params[reg_id].x_offset,
            region_params[reg_id].y_offset,
            region_params[reg_id].width,
            region_params[reg_id].height,
            region_params[reg_id].sensitivity,
            region_params[reg_id].level);
	
	CHECK_ERROR(ret != 0, -1, "set_alert failed");	

	return ret;
}

/* remove a region from motion detection */
int mxuvc_alert_motion_remove_region(int reg_id)
{
	int ret = 0;
	alert_type type;

	CHECK_ERROR(alert_enable == 0, -1,
		"Motion alert module is not initialized");

	type.b.audio_alert = 0;
	type.b.motion_alert = 1;
	 ret = set_alert(type, ALARM_ACTION_REMOVE, 0, reg_id,
	            region_params[reg_id].x_offset,
	            region_params[reg_id].y_offset,
	            region_params[reg_id].width,
	            region_params[reg_id].height,
	            0,
	            0);
	
	CHECK_ERROR(ret != 0, -1, "set_alert failed");

	return ret;
}

int proces_motion_alert(alert_motion_data *malert)
{
	if(alert_enable && motion_callback)
		motion_callback(malert, cb_muser_data);

	return 0;	
}
