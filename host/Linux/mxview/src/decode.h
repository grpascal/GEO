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

#ifndef _DECODE_H_
#define _DECODE_H_

#include "framebuffer.h"

#define DEC_AVC   0
#define DEC_MJPEG 1

struct decode;

struct decode* decode_create(char type);
void decode_destroy(struct decode* dec);
struct framebuffer* decode_frame(struct decode *dec, const unsigned char *frame, signed long length); 

#endif
