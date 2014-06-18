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

#ifndef _COLORSPACE_H_
#define _COLORSPACE_H_

#define FOURCC_NV12 0x3231564e
#define FOURCC_YV12 0x32315659
#define FOURCC_I420 0x30323449
#define FOURCC_YUY2 0x32595559
#define FOURCC_YUYV 0x56595559
#define FOURCC_UYVY 0x59565955

int fourcc_to_pixfmt(int fourcc);

#endif
