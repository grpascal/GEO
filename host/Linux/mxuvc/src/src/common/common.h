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

#ifndef __COMMON_H__
#define __COMMON_H__
#include <string.h>
#include "mxuvc.h"

/* Debug defines */
#define DEBUG_AUDIO_QUEUE 0 /* 1 to enable or 2 for more messages */
#define DEBUG_AUDIO_CALLBACK 0
#define DEBUG_VIDEO_CALLBACK 0

/* Timeout (in ms) for libusb i/o functions */
#define USB_TIMEOUT             5000

#define TRACE(msg, ...)                                               \
	do {                                                          \
		if (MXUVC_TRACE_LEVEL >= 1)                           \
			printf(msg, ##__VA_ARGS__);                   \
	} while(0)
#define TRACE2(msg, ...)                                              \
	do {                                                          \
		if (MXUVC_TRACE_LEVEL >= 2)                           \
			printf(msg, ##__VA_ARGS__);                   \
	} while(0)

#define SHOW(var) TRACE("\t\t" #var " = %i\n", var)
#define SHOW2(var) TRACE2("\t\t" #var " = %i\n", var)

#define WARNING(msg, ...)                                             \
	do {                                                          \
		fprintf(stdout, "WARNING: " msg "\n", ##__VA_ARGS__); \
	} while(0)
#define ERROR(ret_val, msg, ...)                                      \
	do {                                                          \
		fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__);   \
		return ret_val;                                       \
	} while(0)
#define ERROR_NORET(msg, ...)                                         \
	do {                                                          \
		fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__);   \
	} while(0)

#define CHECK_ERROR(cond, ret_val, msg, ...)                       \
	do {                                                       \
		if (cond)                                          \
			ERROR(ret_val, msg,  ##__VA_ARGS__);       \
	} while(0)

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define CST2STR(cst) [cst] = #cst
const char* chan2str(video_channel_t ch);
const char* chan2str_mux(video_channel_t ch);
const char* vidformat2str(video_format_t format);
const char* profile2str(video_profile_t profile);
const char* audformat2str(audio_format_t format);
int next_opt(char *str1, char **opt, char **value);

#define RECORD(fmt, ...) {                                                   \
	extern FILE* debug_recfd;                                            \
	extern int debug_getusleep();                                        \
	if (debug_recfd) {                                                   \
		char str[256];                                               \
		sprintf(str, "usleep(%i);\n", debug_getusleep());            \
		fwrite(str, 1, strlen(str), debug_recfd);                    \
		sprintf(str, "%s(" fmt ");\n", __FUNCTION__, ##__VA_ARGS__); \
		fwrite(str, 1, strlen(str), debug_recfd);                    \
		fflush(debug_recfd);                                         \
	}                                                                    \
}

#ifdef WIN32
#define PACKED
#else
#define PACKED  __attribute__ ((packed))
#endif

typedef enum {
	FW_UPGRADE_START = 0x02,
	FW_UPGRADE_COMPLETE,//0x3
	START_TRANSFER,//0x4
	TRANSFER_COMPLETE,//0x5
	TX_IMAGE,//0x6
	GET_NVM_PGM_STATUS,//0x7
	GET_SNOR_IMG_HEADER,//0x8
	GET_EEPROM_CONFIG,//0x9
	GET_EEPROM_CONFIG_SIZE,//0xA
	REQ_GET_EEPROM_VALUE,//0xB
	GET_EEPROM_VALUE,//0xC
	SET_EEPROM_KEY_VALUE,//0xD
	REMOVE_EEPROM_KEY,//0xE
	SET_OR_REMOVE_KEY_STATUS,//0xF
	ERASE_EEPROM_CONFIG,//0x10
	SAVE_EEPROM_CONFIG,//0x11
	RESET_BOARD,//0x12
	I2C_WRITE,//0x13
	I2C_READ,//0x14
	I2C_WRITE_STATUS, //0x15
	I2C_READ_STATUS, //0x16
	TCW_WRITE,//0x17
	TCW_WRITE_STATUS,//0x18
	TCW_READ,//0x19
	TCW_READ_STATUS,//0x1A
	ISP_WRITE,//0x1B
	ISP_READ,//0x1C
	ISP_WRITE_STATUS,//0x1D
	ISP_READ_STATUS,//0x1E
    	ISP_ENABLE,//0x1F
    	ISP_ENABLE_STATUS,//0x20
	REQ_GET_CCRKEY_VALUE,//0x21
	GET_CCRKEY_VALUE, //0x22
	GET_CCR_SIZE, //0x23
	GET_CCR_LIST, //0x24
	GET_IMG_HEADER,//0x25
	GET_BOOTLOADER_HEADER, //0x26
	GET_CMD_BITMAP, //0x27
	CMD_WHO_R_U, //0x28	
	AV_ALARM, //0x29
	A_INTENSITY,//0x2A
	AUDIO_VAD, //0x2B
	GET_SPKR_VOL, //0x2C
	SET_SPKR_VOL, //0x2D	

	MEMTEST, //0x2E	
	MEMTEST_RESULT, //0x2F

	ADD_FONT, //0x30
	SEND_FONT_FILE, //0x31	
	FONT_FILE_DONE, //0x32
	ADD_TEXT, //0x33
	TEXT, //0x34
	REMOVE_TEXT, //0x35
	SHOW_TIME, //0x36
	HIDE_TIME, //0x37
	UPDATE_TIME, //0x38

	AEC_ENABLE, //0x39,
	AEC_DISABLE, //0x3a,
	AEC_SET_SAMPLERATE, //0x3b,
	AEC_SET_DELAY, //0x3c

	QHAL_VENDOR_REQ_START = 0x60,
	QCC_READ = QHAL_VENDOR_REQ_START,
	QCC_WRITE,
	QCC_READ_STATUS,
	QCC_WRITE_STATUS,
	MEM_READ,
	MEM_WRITE,
	MEM_READ_STATUS,
	MEM_WRITE_STATUS,
	QHAL_VENDOR_REQ_END,

	VENDOR_REQ_LAST = 0x80
} uvc_vendor_req_id_t;

#endif
