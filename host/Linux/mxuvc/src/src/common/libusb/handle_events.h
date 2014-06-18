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

#include <libusb-1.0/libusb.h>

/* The following functions must be used in order to share libusb event
 * handling with the different backends */

/* Call instead of libusb_init() */
int init_libusb(struct libusb_context **ctx);
/* Call instead of libusb_exit() */
int exit_libusb(struct libusb_context **ctx);

int register_libusb_removal_cb(libusb_pollfd_removed_cb removed_cb,
		libusb_device_handle *hdl);
int deregister_libusb_removal_cb(libusb_pollfd_removed_cb removed_cb);

/* Call those functions in order to start/stop libusb event handling */
int start_libusb_events();
int stop_libusb_events();
