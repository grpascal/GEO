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
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/Xinerama.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include "video_window.h"
#include <assert.h>
#include <pthread.h>

Atom wmDeleteMessage;

struct video_window {
	Display         *display;
	int              screen;
	Window           window;
	GC               gc;
	XImage          *ximage;
	XvPortID         port;
	XvImage         *xvimage;

	int		 cp_fourcc;
	XvImageFormatValues *formats;
	int		 num_formats;

	char             *title;
	int              width;
	int              height;

	unsigned int     fullscreen;

	/* framesrc is the incoming video frame
	 * framedsp is the outgoing frame used for display
	 *
	 * If we use X images, we take framesrc, do the sw colorspace conversion,
	 * the sw scaling and store the result in framedsp. XPutImage will take
	 * framedsp as argument.
	 * If we use Xv images, XvPutImage can use framesrc directly since it
	 * does the colorspace conversion and scaling for us.
	 */
	void           *framesrc;
	unsigned int   framesrc_len;
	unsigned int   framesrc_width;
	unsigned int   framesrc_height;

	void           *framedsp; /* width and height are equal
					 to win->width/height since we want to fill
					 the window with the video */
	unsigned int   framedsp_len;

	/* display thread members */
	pthread_t thread;
	struct framebuffer *frm;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
};

void window_show(struct video_window *win)
{
	pthread_mutex_lock(&win->mutex);
	if (win->framesrc_width && win->framesrc_height) {
		win->width = win->framesrc_width;
		win->height= win->framesrc_height;
	}

	/* Make sure the window is not in fullscreen when window_show is called */
	if(win->fullscreen)
		window_toggle_fullscreen(win);

	XResizeWindow(win->display, win->window, win->width, win->height);

	XMapWindow(win->display, win->window);
	XFlush(win->display);
	pthread_mutex_unlock(&win->mutex);
}
void window_hide(struct video_window *win)
{
	pthread_mutex_lock(&win->mutex);
	XUnmapWindow(win->display, win->window);
	XFlush(win->display);
	pthread_mutex_unlock(&win->mutex);
}

static void __window_create(struct video_window *win)
{
	unsigned int black, white;
	unsigned int i;
	int ret;
	XvAdaptorInfo *ai;
	unsigned adaptors;
	XvPortID xvP;

	black = BlackPixel(win->display, win->screen);
	white = WhitePixel(win->display, win->screen);

	win->window = XCreateSimpleWindow(win->display,
				DefaultRootWindow(win->display), 0, 0,
				win->width, win->height, 0,
				white, black);

	// Register to WM close event
	wmDeleteMessage = XInternAtom(win->display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(win->display, win->window, &wmDeleteMessage, 1);

	XSetStandardProperties(win->display, win->window,
				win->title, win->title,
				None, NULL, 0, NULL);

	XSelectInput(win->display, win->window, KeyPressMask);

	win->gc = XCreateGC(win->display, win->window, 0, 0);

	/* Find a displayable adaptor */
	ret = XvQueryAdaptors(win->display, DefaultRootWindow(win->display),
			&adaptors, &ai);
	if (ret != Success) {
		fprintf(stderr, "XvQueryAdaptors failed\n");
		return;
	}

	win->port = 0;
	for (i=0; i < adaptors; i++) {
		if ((ai[i].type & ( XvInputMask | XvImageMask)) == (XvInputMask | XvImageMask)) {
			for (xvP = ai[i].base_id; xvP<ai[i].base_id+ai[i].num_ports; xvP++ ) {
				if (XvGrabPort(win->display, xvP, CurrentTime) == Success) {
					win->port = xvP;
					break;
				}
			}
			if (win->port != 0)
				break;
		}
	}

	if (!win->port) {
		fprintf(stderr, "Failed to grab port\n");
		return;
	}

	/* Check if the Xv adaptor found supports win->cp_fourcc colorspace */
	win->formats = XvListImageFormats(win->display, ai[i].base_id, &win->num_formats);
	XvFreeAdaptorInfo(ai);
}

static int window_supported_fourcc(struct video_window *win, int fourcc)
{
	unsigned int j;
	int adaptor = -1;
	char xv_name[5];
	XvImageFormatValues *formats = win->formats;
	xv_name[4] = 0;
	for(j = 0; j < win->num_formats; j++) {
		memcpy(xv_name,&formats[j].id,4);
		if(formats[j].id == fourcc) {
			fprintf(stderr,"using Xv format 0x%x %s %s\n",
				formats[j].id,
				xv_name,
				(formats[j].format==XvPacked)?"packed":"planar");
			adaptor = 1;
		}
	}
	if (adaptor < 0) {
		return 0;
	}
	return 1;
}

static int window_xinit(struct video_window *win)
{
	win->ximage = XCreateImage(win->display,
			DefaultVisual(win->display, 0),
			24, ZPixmap, 0, NULL,
			win->width, win->height,
			32, 0);
	if (!win->ximage) {
		printf("XCreateImage failed\n");
		return -1;
	}
	printf("Image data: %p, bytes_per_line: %d, bits_per_pixel: %d\n",
			win->ximage->data, win->ximage->bytes_per_line,
			win->ximage->bits_per_pixel);
	printf("red_mask: 0x%lx, blue_mask: 0x%lx, green_mask: 0x%lx\n",
			win->ximage->red_mask, win->ximage->blue_mask,
			win->ximage->green_mask);
	return 0;
}

static int window_xvinit(struct video_window *win)
{
	/* We're good now, let's create the Xv image */
	win->xvimage = XvCreateImage(win->display, win->port, win->cp_fourcc,
						NULL, win->framesrc_width,
						win->framesrc_height);

	if (!win->xvimage) {
		fprintf(stderr, "XvCreateImage failed\n");
		return -1;
	}

	return 0;
}

static void window_set_format(struct video_window *win, unsigned int width, unsigned int height, int fourcc)
{
	/* Frame width/height/colorspace changed since last time */
	if(width != win->framesrc_width || height != win->framesrc_height
			|| fourcc != win->cp_fourcc) {
		if (!win->fullscreen) {
			win->width = width;
			win->height = height;
			XResizeWindow(win->display, win->window, win->width,
					win->height);
		}
		win->framesrc_width = width;
		win->framesrc_height = height;
		win->cp_fourcc = fourcc;
		if(window_supported_fourcc(win, win->cp_fourcc)) {
			if(window_xvinit(win))
				window_xinit(win);
		} else {
			win->xvimage = NULL;
			window_xinit(win);
		}
		XClearWindow(win->display, win->window);
	}
}

static void* __window_thread(void *arg);

struct video_window* window_create(unsigned int width, unsigned int height)
{
	struct video_window *win;

	/* Allocate memory for the window */
	win = calloc(1, sizeof(struct video_window));
	if (win == NULL) {
		perror("malloc");
		goto err_win;
	}

	/* Set the window's parameters */
	win->title = "No frame received";
	win->width = width;
	win->height = height;
	win->cp_fourcc = -1;
	win->xvimage = NULL;
	win->ximage = NULL;
	win->formats = NULL;
	win->num_formats = 0;
	win->fullscreen = 0;

	win->display = XOpenDisplay(NULL);

	if(!win->display) {
		fprintf(stderr, "XOpenDisplay failed\n");
		goto err_win;
	}
	win->screen = DefaultScreen(win->display);

	/* Create viewer window */
	__window_create(win);

	if(pthread_mutex_init(&win->mutex, NULL)) {
		fprintf(stderr, "pthread_mutex_init failed\n");
		goto err_win;
	}
	if(pthread_cond_init(&win->cond, NULL)) {
		fprintf(stderr, "pthread_cond_init failed\n");
		goto err_win;
	}
	if(pthread_create(&win->thread, NULL, __window_thread, win)) {
		fprintf(stderr, "pthread_create failed\n");
		goto err_win;
	}
	return win;

err_win:
	if (win != NULL)
		free(win);
	return NULL;

}

void window_set_title(struct video_window *win, char *title)
{
	pthread_mutex_lock(&win->mutex);
	win->title = title;
	XSetStandardProperties(win->display, win->window,
			win->title, win->title,
			None, NULL, 0, NULL);
	pthread_mutex_unlock(&win->mutex);
}

static int window_getframe(struct video_window *win, struct framebuffer *frm)
{
	int dest_len;
	if( !frm->width || !frm->height) {
		return 0;
	}

	window_set_format(win, frm->width, frm->height, frm->pixelformat);

	win->framesrc = frm->data;
	win->framesrc_width = frm->width;
	win->framesrc_height = frm->height;
	win->framesrc_len = frm->size;

	if(win->xvimage) {
		/* No sw conversion needed, we pass win->framesrc directly */
		win->framedsp = win->framesrc;
		win->framedsp_len = frm->size;
	} else {
		dest_len = win->height*win->ximage->bytes_per_line;
		win->framedsp_len = dest_len;
		win->framedsp = malloc(dest_len);
		if (win->framedsp == NULL) {
			perror("malloc");
			win->framedsp_len = 0;
			return -1;
		}
		/* convert RGB and scale */
		struct SwsContext *sws;
		AVPicture src_frame;
		AVPicture dst_frame;
		enum PixelFormat src_pixfmt = fourcc_to_pixfmt(win->cp_fourcc);
		assert (src_pixfmt != PIX_FMT_NONE);

		sws = sws_getContext(win->framesrc_width, win->framesrc_height,
				src_pixfmt,
				win->width, win->height, PIX_FMT_RGB32,
				SWS_BILINEAR, NULL, NULL, NULL);

		avpicture_fill(&src_frame, (uint8_t*) win->framesrc, src_pixfmt,
				win->framesrc_width, win->framesrc_height);
		avpicture_fill(&dst_frame, (uint8_t*) win->framedsp,
				PIX_FMT_RGB32, win->width, win->height);

		sws_scale(sws, (const uint8_t * const *)src_frame.data, src_frame.linesize,
				0, win->framesrc_height,
				dst_frame.data, dst_frame.linesize);
		sws_freeContext(sws);

		/* we don't need frm->data/win->framesrc  anymore, the result
		 * is in win->framedsp */
		free(frm->data);
	}

	/* framebuffer considered consumed, free it */
	free(frm);

	return 0;
}

static void get_fullscreen_size(struct video_window *win, int *w, int* h)
{
	int xin_screens = -1;
	int i, x_win, y_win;
	XineramaScreenInfo *XinInfo;
	XWindowAttributes xwa;
	Window tmp;

	/* Return the whole X screen size if we're not using Xinerama */
	if (!XineramaIsActive(win->display)) {
		*w = DisplayWidth(win->display, win->screen);
		*h = DisplayHeight(win->display, win->screen);
		return;
	}

	/* We use Xinerama, so we need to figure out what Xinerama screen we are in
	 * and return the size of that screen */

	/* First get the window current position in the X screen */
	XGetWindowAttributes(win->display, win->window, &xwa);
	/* Convert coordinates to absolute coordinates */
	XTranslateCoordinates(win->display, win->window,
			xwa.root, xwa.x, xwa.y,
			&x_win, &y_win, &tmp);

	/* Browse through Xinerama screens */
	XinInfo = XineramaQueryScreens(win->display, &xin_screens);
	/* XineramaIsActive(win->display) returned 1 so the following
	 * should not happened */
	assert((XinInfo != NULL) || (xin_screens != 0));

	printf("x_win=%i, y_win=%i\n", x_win, y_win);
	if (x_win < 0)
		x_win = 0;
	if (y_win < 0)
		y_win = 0;

	for (i = 0; i < xin_screens; i++) {
		/* Check if the window is in this Xinerama screen */
		if(x_win >= XinInfo[i].x_org &&
				x_win <=  XinInfo[i].x_org + XinInfo[i].width &&
				y_win >= XinInfo[i].y_org &&
				y_win <= XinInfo[i].y_org + XinInfo[i].height) {
			*w = XinInfo[i].width;
			*h = XinInfo[i].height;
			printf("Using Xinerama: width=%i, height=%i\n", *w, *h);
			return;
		}
	}

	printf("Video windows not found in any Xinerama screens. Aborting\n");
	abort();
}

void window_toggle_fullscreen(struct video_window *win)
{
	XEvent xev;

	if (win->fullscreen == 1) {
		win->fullscreen=0;
		win->width = win->framesrc_width;
		win->height= win->framesrc_height;
	} else {
		win->fullscreen=1;
		get_fullscreen_size(win, &(win->width), &(win->height));
	}

	Atom wm_state = XInternAtom(win->display, "_NET_WM_STATE", 0);
	Atom fullscreen = XInternAtom(win->display, "_NET_WM_STATE_FULLSCREEN", 0);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = win->window;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = win->fullscreen;
	xev.xclient.data.l[1] = fullscreen;
	xev.xclient.data.l[2] = 0;

	XSendEvent(win->display, DefaultRootWindow(win->display), False,
			SubstructureNotifyMask, &xev);

	XResizeWindow(win->display, win->window, win->width, win->height);

	if(!win->xvimage)
		window_xinit(win);
}

static void check_xevent(struct video_window *win)
{
	int ret;
	extern volatile char stop_capture;
	XEvent ev;

	/* Check Client (WM) events */
	ret = XCheckTypedWindowEvent(win->display, win->window, ClientMessage, &ev);
	if (ret == True && ev.xclient.data.l[0] == wmDeleteMessage)
		stop_capture=1;

	/* Check KeyPress events */
	ret = XCheckTypedWindowEvent(win->display, win->window, KeyPress, &ev);
	if (ret == True) {
		switch (XLookupKeysym(&ev.xkey, 0)) {
		case XK_f: /* f or F */
			window_toggle_fullscreen(win);
			break;;
		case XK_q: /* q or Q */
			stop_capture=1;
			break;;
		default:
			return;
		}
	}
}

static int __window_showframe(struct video_window *win)
{
	int ret;
	if(win->xvimage) {
		win->xvimage->data = win->framedsp;

		ret = XvPutImage(win->display, win->port, win->window,
				win->gc, win->xvimage,
				0, 0, win->framesrc_width, win->framesrc_height,
				0, 0, win->width, win->height);

	} else if (win->ximage) {
		win->ximage->data = win->framedsp;

		ret = XPutImage(win->display, win->window, win->gc, win->ximage,
				0, 0, 0, 0,
				win->width, win->height);
	}

        if (ret != Success) {
		fprintf(stderr, "X(v)PutImage failed\n");
		return -1;
	}
	XFlush(win->display);
	free(win->framedsp);

	return 0;
}

static void* __window_thread(void *arg)
{
	struct video_window *win = arg;
	int ret;
	while(1) {
		ret = pthread_mutex_lock(&win->mutex);
		while(!win->frm) {
			ret = pthread_cond_wait(&win->cond, &win->mutex);
		}
		check_xevent(win);
		if (window_getframe(win, win->frm) >= 0)
			__window_showframe(win);
		win->frm = NULL;
		ret = pthread_mutex_unlock(&win->mutex);
	}
	return NULL;
}

int window_showframe(struct video_window *win, struct framebuffer *frm)
{
#if 1
	if(0 == pthread_mutex_trylock(&win->mutex)) {
		win->frm = frm;
		pthread_cond_signal(&win->cond);
		pthread_mutex_unlock(&win->mutex);
		return 0;
	}
	extern volatile int framedrops;
	framedrops++;
	free(frm->data);
	free(frm);
	return -1;
#else
	check_xevent(win);
	if (window_getframe(win, frm) >= 0)
		return __window_showframe(win);
	return -1;
#endif
}

