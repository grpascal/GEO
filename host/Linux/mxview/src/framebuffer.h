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

#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

struct framebuffer {
	int pixelformat;
	int width;
	int height;
	void *data;
	int size;
};

#include "colorspace.h"

#endif /* _FRAMEBUFFER_H_ */
