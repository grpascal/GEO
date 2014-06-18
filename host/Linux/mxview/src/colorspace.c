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

#include <stdlib.h>
#include <stdio.h>
#include "colorspace.h"
#include <libavcodec/avcodec.h>

int fourcc_to_pixfmt(int fourcc)
{
	switch(fourcc) {
	case FOURCC_I420:
		return PIX_FMT_YUV420P;
	case FOURCC_YUY2:
	case FOURCC_YUYV:
		return PIX_FMT_YUYV422;
	case FOURCC_UYVY:
		return PIX_FMT_UYVY422;
	case FOURCC_NV12:
		return PIX_FMT_NV12;
	default:
		fprintf(stderr,"SW RGB conversion for format 0x%x not supported\n", 
				fourcc);
		return PIX_FMT_NONE;
	}
}
