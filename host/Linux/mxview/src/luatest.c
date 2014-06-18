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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "videodev2.h"
#include <sys/ioctl.h>
#include "uvcext.h"
#include "audio.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "luatest.h"

extern int fd;

static pthread_t        lua_thread;

//FIXME: This should not be in this file. It should be in some header file.
#define V4L2_CID_DIGITIAL_MULTIPLIER			(V4L2_CID_CAMERA_CLASS_BASE+32)
//Lua C macros
#define LUA_PUSH_INTEGER_ON_TABLE(key, value) \
lua_pushinteger(L, value);\
lua_setfield(L, -2, key);\

struct uvc_ctrl {
	char uvc_ctrl_name[25];
	int uvc_ctrl_id;
};
static struct uvc_ctrl uvc_ctrl_data[] = {
	{"Profile", V4L2_CID_XU_AVC_PROFILE},
	{"Level", V4L2_CID_XU_AVC_LEVEL},
	{"Picture Coding", V4L2_CID_XU_PICTURE_CODING},
	{"GOP Structure", V4L2_CID_XU_GOP_STRUCTURE},
	{"GOP Length", V4L2_CID_XU_GOP_LENGTH},
	{"Bitrate", V4L2_CID_XU_BITRATE},
	{"Force I Frame", V4L2_CID_XU_FORCE_I_FRAME},
	{"Version", V4L2_CID_XU_GET_VERSION},
	{"Brightness", V4L2_CID_BRIGHTNESS},
	{"Contrast", V4L2_CID_CONTRAST},
	{"Saturation", V4L2_CID_SATURATION},
	{"Gamma", V4L2_CID_GAMMA},
	{"Hue", V4L2_CID_HUE},
	{"Gain", V4L2_CID_GAIN},
	{"Sharpness", V4L2_CID_SHARPNESS},
	{"Backlight_Compensation", V4L2_CID_BACKLIGHT_COMPENSATION},
	{"Power_line_frequency", V4L2_CID_POWER_LINE_FREQUENCY},
	{"Pan", V4L2_CID_PAN_ABSOLUTE},
	{"Tilt",  V4L2_CID_TILT_ABSOLUTE},
	{"Zoom", V4L2_CID_DIGITIAL_MULTIPLIER},

};

static void _nanosleep(time_t sec, long nsec)
{
	struct timespec tim1, tim2;
	tim1.tv_sec = sec;
	tim1.tv_nsec = nsec;
	nanosleep(&tim1, &tim2);
}

static void _wait(void)
{
	_nanosleep(0,1000);
}

static int pusherror(lua_State *L, const char *msg)
{
	lua_pushnil(L);
	lua_pushfstring(L, "%s", msg);
	return 2;
}

static int pushresult(lua_State *L, int ret)
{
	int e = errno;
	if (ret >= 0) {
		lua_pushboolean(L, 1);
		return 1;
	} else {
		return pusherror(L, strerror(e));
	}
}


static int set_resolution(lua_State *L)
{
	const int width = luaL_checkinteger(L, 1);
	const int height = luaL_checkinteger(L, 2);
	extern volatile int change_res;
	extern struct v4l2_frmsize_discrete res[50];
	extern struct v4l2_format fmt;
	extern int nres;
	int i;

	/* Use traditional resolution switching for non AVC formats*/
	if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MPEG
		&& fmt.fmt.pix.pixelformat != v4l2_fourcc('M','P','2','T')
		&& fmt.fmt.pix.pixelformat != v4l2_fourcc('H','2','6','4')) {

		for (i = 0; i < nres; i++) {
			if(res[i].width == width && res[i].height == height) {
				change_res = i+1;
				while(change_res) _wait();
				return pushresult(L, 1);
			}
		}
		return pusherror(L, "Unsupported resolution");
	}

	/* AVC formats use the XU resolution control */
	for (i = 0; i < NUM_XU_RES; i++) {
		if(xu_res_mapping[i].id2 == ((width<<16)|height)) {
			if(!set_v4l2_ctrl(V4L2_CID_XU_RESOLUTION2, xu_res_mapping[i].id2))
				return pushresult(L, 1);
			else if(!set_v4l2_ctrl(V4L2_CID_XU_RESOLUTION, xu_res_mapping[i].id))
				return pushresult(L, 1);
			/* double fail - break out */
			return pushresult(L, -1);
		}
	}

	return pusherror(L, "Unsupported resolution");
}

static int set_video_format(lua_State *L)
{
	const int pixfmt = luaL_checkinteger(L, 1);
	extern struct v4l2_fmtdesc fmtd[];
	extern int nfmt;
	extern volatile int change_fmt;
	int i;

	for(i=0; i<nfmt; i++) {
		if(fmtd[i].pixelformat != pixfmt)
			continue;
		change_fmt = i + 1;
		return pushresult(L, 1);
	}
	return pusherror(L, "Unsupported resolution");
}


static int getintfield(lua_State *L, const char *key)
{
	int res;
	lua_getfield(L, -1, key);
	if (lua_isnil(L, -1))
		return luaL_argerror(L, 1, lua_pushfstring(L, "field " LUA_QS " missing in table", key));
	if (!lua_isnumber(L, -1))
		return luaL_argerror(L, 1, lua_pushfstring(L, "field " LUA_QS " number expected, got %s", key, luaL_typename(L, -1)));
	res = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return res;
}

static const char* getstringfield(lua_State *L, const char *key)
{
	const char *res;
	lua_getfield(L, -1, key);
	if (lua_isnil(L, -1))
		luaL_argerror(L, 1, lua_pushfstring(L, "field " LUA_QS " missing in table", key));
	if (!lua_isstring(L, -1))
		luaL_argerror(L, 1, lua_pushfstring(L, "field " LUA_QS " string expected, got %s", key, luaL_typename(L, -1)));
	res = lua_tostring(L, -1);
	lua_pop(L, 1);
	return res;
}

static int set_uvc(lua_State *L)
{
	const char *func_name;
	int func_value;
	int i;
	int value;
	extern volatile int change_fps;
	extern volatile int change_fmt;
	extern volatile int change_res;
	extern struct v4l2_fmtdesc fmtd[];
	extern int nfmt;

	if (lua_istable(L, 1)) {
		lua_settop(L, 1);
		func_name = getstringfield(L, "param");
		func_value = getintfield(L, "value");
	} else {
		func_name = luaL_checkstring(L, 1);
		func_value = luaL_checkinteger(L, 2);
	}

	/* Change framerate */
	if (strcmp(func_name, "Framerate") == 0) {
		value = set_v4l2_framerate(func_value);
		if (value < 0) {
			change_fps = func_value + 1;
			while (change_fps) _wait();
			/* fall thru success */
		}
		return pushresult(L, 1);
	}

	/* Change pixel format */
	if (strcmp(func_name, "Pixelformat") == 0) {
		for (i = 0; i < nfmt; i++) {
			printf(" %s\n", fmtd[i].description);
		}
		change_fmt = func_value + 1;
		while(change_fmt) _wait();
		return pushresult(L, 1);
	}

	if (strcmp(func_name, "Resolution") == 0 ) {
		lua_settop(L, 0);
		lua_pushinteger(L, func_value >> 16);
		lua_pushinteger(L, func_value & 0xffff);
		return set_resolution(L);
	}

	/* Other controls */
	for (i = 0; i < sizeof(uvc_ctrl_data)/sizeof(uvc_ctrl_data[0]); i++) {
		if (strcmp(func_name, uvc_ctrl_data[i].uvc_ctrl_name) == 0) {
			int ret = set_v4l2_ctrl(uvc_ctrl_data[i].uvc_ctrl_id, func_value);
			return pushresult(L, ret);
		}
	}

	/* If we reach here there is no such func_name, this is an error */
	return luaL_argerror(L, 1, lua_pushfstring(L, "invalid option " LUA_QS, func_name));
}

static int get_resolution(lua_State *L)
{
	extern struct v4l2_format fmt;
	int result;

	/* Non AVC formats*/
	if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MPEG
		&& fmt.fmt.pix.pixelformat != v4l2_fourcc('M','P','2','T')
		&& fmt.fmt.pix.pixelformat != v4l2_fourcc('H','2','6','4')) {

		lua_pushnumber(L, fmt.fmt.pix.width);
		lua_pushnumber(L, fmt.fmt.pix.height);
		return 2;
	}
	/* AVC formats: new guid */
	if(!get_v4l2_ctrl(V4L2_CID_XU_RESOLUTION2, &result)) {
		lua_pushnumber(L, result >> 16);
		lua_pushnumber(L, result & 0xffff);
		return 2;
	}

	/* AVC formats: old guid */
	if(!get_v4l2_ctrl(V4L2_CID_XU_RESOLUTION, &result)) {
		int i;
		for (i=0; i < NUM_XU_RES; i++) {
			if (xu_res_mapping[i].id == result) {
				lua_pushnumber(L, xu_res_mapping[i].id2 >> 16);
				lua_pushnumber(L, xu_res_mapping[i].id2 & 0xffff);
				return 2;
			}
		}
	}

	return 0;
}

static int get_uvc(lua_State *L)
{
	const char *func_name;
	int i;
	int result = 0;
	extern struct v4l2_format fmt;

	if (lua_istable(L, 1)) {
		lua_settop(L, 1);
		func_name = getstringfield(L, "param");
	} else {
		func_name = luaL_checkstring(L, 1);
	}

	/* Get framerate */
	if (strcmp(func_name, "Framerate") == 0) {
		result = get_v4l2_framerate();
		lua_pushnumber(L, result);
		return 1;
	}

	if (strcmp(func_name, "Resolution") == 0 ) {
		return get_resolution(L);
	}

	/* Other controls */
	for (i = 0; i < sizeof(uvc_ctrl_data)/sizeof(uvc_ctrl_data[0]); i++) {
		if (strcmp(func_name, uvc_ctrl_data[i].uvc_ctrl_name) == 0) {
			int ret = get_v4l2_ctrl(uvc_ctrl_data[i].uvc_ctrl_id, &result);
			if(ret >= 0) {
				lua_pushnumber(L, result);
				return 1;
			}
			return pushresult(L, ret);
		}
	}

	/* If we reach here there is no such func_name, this is an error */
	return luaL_argerror(L, 1, lua_pushfstring(L, "invalid option " LUA_QS, func_name));
}

int lsleep(lua_State * L)
{
	/* first 'sec' */
	_nanosleep(lua_tointeger(L, 1), 0);
	return 0;
}

int lnanosleep(lua_State * L)
{
	/* [last-1 'sec'], last 'nsec' */
	_nanosleep(lua_tointeger(L, -2), lua_tointeger(L, -1));
	return 0;
}

#ifndef NOAUDIO
int startaudio(lua_State * L)
{
	char *audiodev = (char*) luaL_checkstring(L, 1);
	start_audio(audiodev);
	lua_pushboolean(L, 1);
	return 1;
}

int stopaudio(lua_State * L)
{
	stop_audio();
	lua_pushboolean(L, 1);
	return 1;
}

int reset_audiostats(lua_State * L)
{
	extern volatile char reset_stats;
	reset_stats = 1;
	lua_pushboolean(L, 1);
	return 1;
}

int get_audiostats(lua_State * L)
{
	extern volatile long aud_frame, aud_frame_live;
	extern volatile long aud_nzero, aud_nzero_live;
	extern volatile int aud_nzero_maxseq;
	extern volatile int aud_maxsize;
	lua_newtable(L);
	LUA_PUSH_INTEGER_ON_TABLE("Audframes", aud_frame);
	LUA_PUSH_INTEGER_ON_TABLE("Zero-sizeaudframeslive", aud_frame_live);
	LUA_PUSH_INTEGER_ON_TABLE("Zero-sizeaud_fr_%",
				  (100 * (double)aud_nzero /
				   (double)aud_frame));
	LUA_PUSH_INTEGER_ON_TABLE("zero-sizeaudfr_live_%",
				  (100 * aud_nzero_live / aud_frame_live));
	LUA_PUSH_INTEGER_ON_TABLE("Maxaudframe", aud_nzero_maxseq);
	LUA_PUSH_INTEGER_ON_TABLE("Maxaudframesize", aud_maxsize);
	return 1;
}
#endif

static void *lua_start(void *ptr)
{
	lua_State *L;
	char *lua_filename = ptr;
	L = lua_open();
	luaL_openlibs(L);
	lua_register(L, "set_resolution", set_resolution);
	lua_register(L, "set_uvc", set_uvc);
	lua_register(L, "get_resolution", get_resolution);
	lua_register(L, "get_uvc", get_uvc);
	lua_register(L, "sleep", lsleep);
	lua_register(L, "nanosleep", lnanosleep);
#ifndef NOAUDIO
	lua_register(L, "startaudio", startaudio);
	lua_register(L, "stopaudio", stopaudio);
	lua_register(L, "reset_audiostats", reset_audiostats);
	lua_register(L, "get_audiostats", get_audiostats);
#endif
	lua_register(L, "set_video_format", set_video_format);

	/* Register Video Formats */
	lua_pushinteger(L, V4L2_PIX_FMT_MPEG);
	lua_setglobal (L, "VID_FMT_MPEG");
	lua_pushinteger(L, v4l2_fourcc('M','P','2','T'));
	lua_setglobal (L, "VID_FMT_MP2T");
	lua_pushinteger(L, v4l2_fourcc('H','2','6','4'));
	lua_setglobal (L, "VID_FMT_H264");
	lua_pushinteger(L, V4L2_PIX_FMT_MJPEG);
	lua_setglobal (L, "VID_FMT_MJPEG");
	lua_pushinteger(L, v4l2_fourcc('Y','U','Y','2'));
	lua_setglobal (L, "VID_FMT_YUY2");
	lua_pushinteger(L, v4l2_fourcc('Y','U','Y','V'));
	lua_setglobal (L, "VID_FMT_YUYV");
	lua_pushinteger(L, v4l2_fourcc('N','V','1','2'));
	lua_setglobal (L, "VID_FMT_NV12");

	if (luaL_dofile(L, lua_filename)) {
		printf("%s\n", lua_tostring(L, -1));
	}

	return NULL;
}

void lua_init(char* lua_filename) {
	pthread_create(&lua_thread, NULL, &lua_start, lua_filename);
}
