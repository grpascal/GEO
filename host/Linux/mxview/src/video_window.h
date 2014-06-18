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

#ifndef _VIDEOWINDOW_H_
#define _VIDEOWINDOW_H_

#include "framebuffer.h"

struct video_window;

struct video_window* window_create(unsigned int width, unsigned int height);
void window_set_title(struct video_window *win, char *title);
int window_showframe(struct video_window *win, struct framebuffer* frm);
void window_show(struct video_window *win); 
void window_hide(struct video_window *win);
void window_toggle_fullscreen(struct video_window *win);

#endif
