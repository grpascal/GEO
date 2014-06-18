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

#ifndef __ALERT_H__
#define __ALERT_H__

#include <mxuvc.h>

typedef union {
	uint32_t	d32;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	struct {
        unsigned int audio_alert : 1;
        unsigned int motion_alert: 1;
	unsigned int reserved : 30;
	} b;
#elif __BYTE_ORDER == __BIG_ENDIAN
	struct {
	unsigned int reserved : 30;
        unsigned int motion_alert: 1;
        unsigned int audio_alert : 1;
	} b;
#endif
}alert_type;

typedef enum{
	ALARM_ACTION_ENABLE = 1, //enable alarm
	ALARM_ACTION_DISABLE,      //disable alarm
	ALARM_ACTION_ADD,              //add a new value to the alarm control
	ALARM_ACTION_REMOVE,     //remove a  value to the alarm control
	NO_ACTION, //TBD
}alert_action;

typedef struct {
	uint32_t audioIntensityDB;
}audio_alert_data;

//functions
int get_audio_intensity(uint32_t *audioIntensityDB);

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
);

int proces_audio_alert(audio_alert_data *aalert);
int proces_motion_alert(alert_motion_data *malert);

int check_alert_state(void);
#endif
