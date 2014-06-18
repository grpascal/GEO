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
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "videodev2.h"
#include "uvcext.h"
#include <signal.h>
#include "audio.h"
#include "luatest.h"


#ifndef NOX
#include "decode.h"
#include "video_window.h"
#include "gui.h"

static struct video_window* win;
static struct decode*	dec;
static int 		dec_type = -1;
static int		raw_fourcc;

//#define SHOWBITRATE
#define BITRATE_MEASUREMENT_WINDOW (1) // seconds
/* Measured in bits/second */
int videoTransportStreamBitrate = 0;
/* Measured in bits/second */
int videoElementaryStreamBitrate = 0;
/* duration over which to measure bitrate in seconds */
static int videoBitrateWindowSize = BITRATE_MEASUREMENT_WINDOW;
static int videoTransportStreamBitsThisWindow = 0;
static int videoElementaryStreamBitsThisWindow = 0;
static int videoFramesThisWindow = 0;
#endif


#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct buffer {
	void   *start;
	size_t  length;
};

#define CAPTURE_STOPPED 0
#define CAPTURE_STARTED 1
volatile char capture_state = CAPTURE_STARTED;
volatile char start_capture = 0;
volatile char stop_capture = 0;
volatile char snapshot_clicked = 0;
#define AUDIO_STOPPED 0
#define AUDIO_STARTED 1
volatile char audio_state = AUDIO_STOPPED;
volatile char toggle_audio = 0;

volatile int change_fmt = 0;
volatile int change_res = 0;
volatile int change_fps = 0;
volatile char display_xu = 0;
volatile char puxu_anf_en = 0;
volatile char puxu_awdr_en = 0;
volatile char puxu_ae_en = 0;
volatile char puxu_awb_en = 0;
volatile char puxu_wbzone_en = 0;
volatile char puxu_expzone_en = 0;
volatile char reset_res;
volatile char reset_fr;
volatile double cur_framerate;
volatile int framedrops;
volatile char *alsa_device_capture = "hw:0,0";
char *filename_audio = NULL;
char *filename_video = NULL;

static char            *dev_name;
int  	                fd = -1;
struct buffer          *buffers;
static unsigned int     n_buffers;
static int		out_buf;
static int              force_format;
static unsigned		fourcc;
static int              frame_count = 0;
static int              buffer_count = 8;
static int              time_count = 0;
static int 		width;
static int 		height;
static void (*display_func)(const void *buf, int len);
static int lastcc = -1;

struct v4l2_format fmt;
struct v4l2_frmsize_discrete res[50];
int cres = 0;
int nres = 0;

struct v4l2_fmtdesc fmtd[50];
int cfmt = 0;
int nfmt = 0;

static int start_in_fullscreen = 0;
static int nogui = 0;

static void errno_exit(const char *s)
{
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
	int r;

	do {
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}


#ifndef NOX
static void raw_display(const void *buf, int len)
{
	struct framebuffer *frm;
	frm = malloc(sizeof(struct framebuffer));

	frm->pixelformat = raw_fourcc;
	frm->width = width;
	frm->height = height;
	frm->size = len;
	frm->data = malloc(len);
	memcpy(frm->data, buf, len);
	window_showframe(win, frm);
}

static void decode(const void *buf, int len)
{
	struct framebuffer *frm;
        static struct timeval timestart, timeend;
        int deltatime;

        // initialize time if bits in window is zero - handles startup
        if (videoElementaryStreamBitsThisWindow == 0)
        {
            // initialize the start of the window
            gettimeofday(&timestart, NULL);
        }
        else
        {
            // get the current time
            gettimeofday(&timeend, NULL);

            deltatime = (timeend.tv_sec-timestart.tv_sec) * 1000000 +
                        (timeend.tv_usec-timestart.tv_usec);

            // check if measurement window has expired
            if (deltatime > videoBitrateWindowSize * 1000000)
            {
                videoTransportStreamBitrate = videoTransportStreamBitsThisWindow/videoBitrateWindowSize;
                videoElementaryStreamBitrate = videoElementaryStreamBitsThisWindow/videoBitrateWindowSize;

#ifdef SHOWBITRATE
                printf("ts stream bps %d elem stream bps %d frames %d\n", 
                       videoTransportStreamBitrate, videoElementaryStreamBitrate, videoFramesThisWindow);
#endif

                // initialize the start of the window
                gettimeofday(&timestart, NULL);
            
                // zero window counts
                videoTransportStreamBitsThisWindow = 0;
                videoElementaryStreamBitsThisWindow = 0;
                videoFramesThisWindow = 0;
            }
        }

        // count the bits in the stream
        videoElementaryStreamBitsThisWindow += len*8;

        // count number of frames
        videoFramesThisWindow++;

	frm = decode_frame(dec, buf, len);
	if(frm) {
		window_showframe(win, frm);
	}
}

static void demux_ts(const void *buf, int len)
{
        static long long lastbytepos = 0;
        static long long bytepos = 0;
	int i, ns = 0;
	void *nal = malloc(len);

	/* demux ts stream */
	for( i = 0; i < len; i+= 188)
	{
		const unsigned char *p = buf + i;
		int start = p[1] & 0x40;
		int pid = (((int)p[1] & 0x1f) << 8) | p[2];

                // add up bits in the transport stream ignoring the trailer in the uvc packet
                // but including all PIDs:
                videoTransportStreamBitsThisWindow += 188*8;

		// FIXME - hardcoded PID
		if( pid != 0x1011 )
			continue;
		int af = p[3] & 0x20;
		int pl = p[3] & 0x10;
                int cc = p[3] & 0x0f;
                // printf("lastcc %d cc %d pl %d\n", lastcc, cc, pl);
                if ((lastcc != -1) && (pl == 0x10) && (((lastcc+1) & 0xf) != cc))
                {
                        printf("** MPEG2-TS continuity error ");
			printf("(expected %d vs %d) at byte %lld\n", (lastcc+1) & 0xf, cc, bytepos+i);
			printf("   (%lld bytes/%lld packets from last error)\n", bytepos+i-lastbytepos, (bytepos+i-lastbytepos)/188);
                }
                lastcc = cc;
                lastbytepos = bytepos;
		int ps = 184;
		if(!pl)
			continue;

		p += 4;
		if(af) {
			ps -= p[0] + 1;
			p += p[0] + 1;
		}
		// PES is here
		if(start) {
			if(ns) {
				decode(nal, ns);
				ns = 0;
			}
			// FIXME is this always 20?
			ps -= 20;
			p += 20;
		}
		memcpy(nal+ns, p, ps);
		ns += ps;
	}
	decode(nal, ns);
	free(nal);
        bytepos += len;

}

struct uvc_skypexu_stream_hdr{
	char			pts[8];
	char			stream_id;
	char			stream_type;
	unsigned short int	counter;
	unsigned int		payload_offset;
	unsigned int		payload_size;
};

int skypexu_active = 0;

void print_skypefooter(char *buf, int len)
{
	struct uvc_skypexu_stream_hdr *hdr = 
			(struct uvc_skypexu_stream_hdr *)(buf + len - 28);
	printf("\nbuffer length %d\n",len);
	printf("----------------\n");
	printf("pts		%c%c%c%c%c%c%c%c\n",hdr->pts[0],
				hdr->pts[1],hdr->pts[2],hdr->pts[3],
				hdr->pts[4],hdr->pts[5],hdr->pts[6],
				hdr->pts[7]);
	printf("StreamID 	%x\n",hdr->stream_id);
	printf("StreamType 	%x\n",hdr->stream_type);
	printf("Sequence 	%x\n",hdr->counter);
	printf("PayloadOffset 	%d\n",hdr->payload_offset);
	printf("PayloadSize 	%d\n",hdr->payload_size);
	printf("----------------\n");
	printf("num headers %x\n",*(buf + len - 8));
}

static void demux_skype(const void *buf, int len)
{
	struct uvc_skypexu_stream_hdr *hdr;
	unsigned int num_hdrs = 0;
	char *buffer_offset;
	/* print_skypefooter((char *)buf, len);*/

	/* FIXME - actually demux and select a payload stream/format */
	if(dec_type != DEC_AVC) {
		if(dec)
			decode_destroy(dec);
		dec = decode_create(dec_type = DEC_AVC);
		window_set_title(win, "Skype Transport AVC Decoder");
	}
	/* FIXME - drop Skype footer by dead reckoning */
	/* drop 20Byte Header + 8Byte Magic & N */
	//decode footer & find the correct length of valid frame
	num_hdrs = *(int *)((char *)buf + len - 8);
	
	//if(num_hdrs == 1){	
		hdr = (struct uvc_skypexu_stream_hdr *)((char *)buf + len - 28);
		//buffer_offset = (char *)buf + hdr->payload_offset;
		decode((void *)buf, len - 28);
		//decode(buf, len-28);
//	}else
	//	printf("Muxed Stream support is not implemented\n");
		
	//decode(buf, len-28);
}

static void demux(const void *buf, int len)
{
	if(!len)
		return;
	if(len > 28 && !strncmp(buf+len-4, "SKYP", 4))
		demux_skype(buf, len);
	else if( *(char*)buf == 0x47 && len % 188 == 0 )
		demux_ts(buf, len);
	else
		decode(buf, len);
}

static void init_display(void)
{
	display_xu = 2; /* Hide the extension controls tab in the GUI */
	width = fmt.fmt.pix.width;
	height = fmt.fmt.pix.height;
	if(!width)
		width = 640;
	if(!height)
		height = 480;
	if(!win)
		win = window_create(width, height);
	if(dec) {
		decode_destroy(dec);
		dec = NULL;
	}
	/* Initialize the decoder and create the window for the display */
	if(fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG
			|| fmt.fmt.pix.pixelformat == v4l2_fourcc('J','P','E','G')) {
		dec = decode_create(dec_type = DEC_MJPEG);
		window_set_title(win, "MJPEG Decoder");
		display_func = demux;
	} else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MPEG
			|| fmt.fmt.pix.pixelformat == v4l2_fourcc('M','P','2','T')) {
		dec = decode_create(dec_type = DEC_AVC);
		window_set_title(win, "AVC Decoder");
		display_func = demux;
		/* Display the extension controls tab in the GUI */
		display_xu = 1;
	} else if (fmt.fmt.pix.pixelformat == v4l2_fourcc('H','2','6','4')) {
		dec = decode_create(dec_type = DEC_AVC);
		if(skypexu_active == 1){
			printf("skypexu mode activated\n");
			window_set_title(win, "Skype Transport AVC Decoder");	
			skypexu_active = 0;
		}else
			window_set_title(win, "AVC Decoder");
		display_func = demux;
		/* Display the extension controls tab in the GUI */
		display_xu = 1;
	} else if (fmt.fmt.pix.pixelformat == v4l2_fourcc('Y','U','Y','2')) {
		window_set_title(win, "Raw YUY2");
		display_func = raw_display;
		raw_fourcc = FOURCC_YUY2;
	} else if (fmt.fmt.pix.pixelformat == v4l2_fourcc('Y','U','Y','V')) {
		window_set_title(win, "Raw YUYV");
		display_func = raw_display;
		raw_fourcc = FOURCC_YUY2; /* Alias of YUYV */
	} else if (fmt.fmt.pix.pixelformat == v4l2_fourcc('N','V','1','2')) {
		window_set_title(win, "Raw NV12");
		display_func = raw_display;
		raw_fourcc = FOURCC_NV12;
	} else {
		char fourcc[5];
		memcpy(fourcc, &fmt.fmt.pix.pixelformat, 4);
		fourcc[4] = 0;
		printf("Unsupported pixel format %s, display disabled\n", fourcc);
	}
}

#else
#define init_display() {}
#define window_show(a) {}
#define window_hide(a) {}
#endif

static void process_image(const void *p, int size)
{
	static FILE *f;
	if (out_buf) {
		if(!f) {
			f = fopen(filename_video,"wb");
			if (f == NULL) {
				perror("ERROR: Cannot open output file for writing");
				out_buf = 0;
				printf("Writing to ouput file disabled");
			}
		}
		if (f)
			fwrite(p, size, 1, f);
	}
	if(display_func)
		display_func(p, size);
}

static void process_snapshot(const void *p, int size)
{
    FILE *snapshot_file;
    static int filename_cnt = 0;
    char filename[256];
    memset(filename, 0, 256);
    char *pdest = NULL;
    char *psrc;
    int i;

    if(fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG
            || fmt.fmt.pix.pixelformat == v4l2_fourcc('J','P','E','G')) {
        sprintf(filename, "snapshot%06d.jpg", filename_cnt++);
    } else if (fmt.fmt.pix.pixelformat == v4l2_fourcc('Y','U','Y','2')
            || fmt.fmt.pix.pixelformat == v4l2_fourcc('Y','U','Y','V'))	
    {
        sprintf(filename, "snapshot%06d.yuv", filename_cnt++);

        // we need to swap color components for YUY2 since packing is not 601.  This
        // buffer is YUYV and we need to be UYVY
        pdest = (char *)malloc(size);
        psrc = (char *)p;
        for (i = 0; i < size; i+=2)
        {
            pdest[i] = psrc[i+1];
            pdest[i+1] = psrc[i];
        }
    }
    else if ( fmt.fmt.pix.pixelformat == v4l2_fourcc('N','V','1','2')) 
    {
        sprintf(filename, "snapshot%06d.yuv", filename_cnt++);
    }
    else
        return;


    snapshot_file = fopen(filename, "wb");
    if (snapshot_file == NULL) {
        perror("ERROR: Cannot open snapshot file for writing");
        printf("Writing to snapshot file disabled");
        return;
    }
    if (pdest != NULL)
    {
        fwrite(pdest, size, 1, snapshot_file);
        free(pdest);
    }
    else
    {
        fwrite(p, size, 1, snapshot_file);
    }
    fclose(snapshot_file);
}

static int read_frame(void)
{
	struct v4l2_buffer buf;
	static int sequence = -1;

	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */
				return 0;

			default:
				perror("VIDIOC_DQBUF");
                exit(1);
		}
	}

	if (buf.sequence && buf.sequence != sequence + 1) {
		printf("sequence mismatch expected %d got %d - frames were missed, expect errors or encoder/decoder drift\n", sequence+1, buf.sequence);
	}
	sequence = buf.sequence;

	assert(buf.index < n_buffers);

	/* Snapshot when MJPEG or YUY2 */
	if ( snapshot_clicked )
	{
		process_snapshot(buffers[buf.index].start, buf.bytesused);
		snapshot_clicked = 0;
	}

	process_image(buffers[buf.index].start, buf.bytesused);

	if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
		perror("VIDIOC_QBUF");

	return 1;
}


static void stop_capturing(void)
{
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
		errno_exit("VIDIOC_STREAMOFF");
}

static void start_capturing(void)
{
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
			errno_exit("VIDIOC_QBUF");
	}
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
		errno_exit("VIDIOC_STREAMON");
}

static void uninit_mmap(void)
{
	struct v4l2_requestbuffers req;
	unsigned int i;

	for (i = 0; i < n_buffers; ++i)
		if (-1 == munmap(buffers[i].start, buffers[i].length))
			errno_exit("munmap");

	free(buffers);

	CLEAR(req);

	req.count = 0;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
				 "memory mapping\n", dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}
}

static void init_mmap(void)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = buffer_count;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
				 "memory mapping\n", dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n",
			 dev_name);
		exit(EXIT_FAILURE);
	}

	buffers = calloc(req.count, sizeof(*buffers));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;

		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
			errno_exit("VIDIOC_QUERYBUF");

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start =
			mmap(NULL /* start anywhere */,
			      buf.length,
			      PROT_READ | PROT_WRITE /* required */,
			      MAP_SHARED /* recommended */,
			      fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start)
			errno_exit("mmap");

		memset(buffers[n_buffers].start, 0xab, buf.length);
	}
}

static void enumerate_formats(void)
{
	struct v4l2_fmtdesc fmtdesc;

	nfmt = 0;
	cfmt = 0;
	memset(fmtd, 0, sizeof fmtd);

	memset(&fmtdesc, 0, sizeof(fmtdesc));
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while( ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0 ) {
		printf(" %s\n", fmtdesc.description);
		if(fmtdesc.pixelformat == fmt.fmt.pix.pixelformat)
			cfmt = nfmt;
		fmtd[nfmt++] = fmtdesc;
		fmtdesc.index++;
	}
}

static int cmp_frmsize(const void* a, const void *b)
{
	const struct v4l2_frmsize_discrete *x = a;
	const struct v4l2_frmsize_discrete *y = b;
	return x->width*x->height - y->width*y->height;
}

static void enumerate_res(void)
{
	int i;
	struct v4l2_pix_format pix;
	struct v4l2_frmsizeenum frmsize;

	nres = 0;

	pix = fmt.fmt.pix;
	printf(" Current %ux%u\n", pix.width, pix.height);

	/* Skip out early for MP2TS format */
	if(!pix.width && !pix.height)
		return;

	frmsize.index = 0;
	frmsize.pixel_format = pix.pixelformat;
	while(xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
		res[frmsize.index++] = frmsize.discrete;
	}
	nres = frmsize.index;
	qsort(res, nres, sizeof(res[0]), cmp_frmsize);
	/* List sorted resolutions */
	for(i = 0; i < nres; i++) {
		printf(" Found %ux%u\n", res[i].width, res[i].height);
		if(res[i].width == pix.width && res[i].height == pix.height)
			cres = i;
	}
}

static void init_device(void)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	unsigned int min;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n",
				 dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n",
			 dev_name);
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n",
			 dev_name);
		exit(EXIT_FAILURE);
	}


	/* Select video input, video standard and tune here. */


	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
			case EINVAL:
				/* Cropping not supported. */
				break;
			default:
				/* Errors ignored. */
				break;
			}
		}
	} else {
		/* Errors ignored. */
	}


	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (force_format) {
		fmt.fmt.pix.width       = 640;
		fmt.fmt.pix.height      = 480;
		fmt.fmt.pix.pixelformat = fourcc;
		fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

		if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)){
			errno_exit("VIDIOC_S_FMT");
			return;
		}

		/* Note VIDIOC_S_FMT may change width and height. */
	} else {
		/* Preserve original settings as set by v4l2-ctl for example */
		if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
			errno_exit("VIDIOC_G_FMT");
	}

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	init_mmap();
	init_ctrl(fd);
}


static void close_device(void)
{
	if (-1 == close(fd))
		errno_exit("close");

	fd = -1;
}

static void open_device(void)
{
	struct stat st;

	if (-1 == stat(dev_name, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n",
			 dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", dev_name);
		exit(EXIT_FAILURE);
	}

	fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

	if (-1 == fd) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n",
			 dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static void mainloop(void)
{
	unsigned int count;
	struct timeval tv, tv0, tvdone;
	unsigned int sum = 0;
	unsigned int cnt = 0;

	count = frame_count;
	gettimeofday(&tv, NULL);
	tv0 = tv;
	tvdone = tv;
	tvdone.tv_sec += time_count;

	while ((frame_count == 0 || count-- > 0) && (time_count == 0 || timercmp(&tv,&tvdone,<))) {
		for (;;) {
			fd_set fds;
			int r;

			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			if (capture_state == CAPTURE_STARTED) {
				/* Set timeout */
				tv.tv_sec = 5;
				tv.tv_usec = 0;

				r = select(fd + 1, &fds, NULL, NULL, &tv);

				if (-1 == r) {
					if (EINTR == errno)
						continue;
					errno_exit("select");
				}

				if (0 == r) {
					fprintf(stderr, "select timeout\n");
					exit(EXIT_FAILURE);
				}

				gettimeofday(&tv, NULL);
				sum = (tv.tv_sec*1000000
						+ tv.tv_usec
						- tv0.tv_sec*1000000
						- tv0.tv_usec);
				cnt++;
				if(sum > 500000) {
					cur_framerate = 1e6*cnt/sum;
					tv0 = tv;
					cnt = 0;
				}
			} else {
				/* Wait a little bit (1/30th of a second) in
				 * order not to take all the cpu time */
				struct timespec tim, tim2;
				tim.tv_sec = 0;
				tim.tv_nsec = 33000000; /* 33 ms */
				nanosleep(&tim , &tim2);
			}

#ifndef NOAUDIO
			if (toggle_audio) {
				if (audio_state == AUDIO_STOPPED) {
					start_audio((char *) alsa_device_capture);
					audio_state = AUDIO_STARTED;
				} else {
					stop_audio();
					audio_state = AUDIO_STOPPED;
				}
				toggle_audio = 0;
			}
#endif
			if (change_fmt || change_res || change_fps ||
					start_capture || stop_capture) {
				if (capture_state == CAPTURE_STARTED)
					stop_capturing();
				uninit_mmap();
				/* Change resolution */
				if(change_res) {
					cres = change_res-1;
					width  = fmt.fmt.pix.width = res[cres].width;
					height = fmt.fmt.pix.height = res[cres].height;
					if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)){
						perror("VIDIOC_S_FMT");
						return;
					}
					printf("change_res = %d\n", change_res);
					change_res = 0;
				}
				/* Change pixel format */
				if(change_fmt) {
					cfmt = change_fmt-1;
					fmt.fmt.pix.pixelformat = fmtd[cfmt].pixelformat;
					/* Use cres to make the switch attempt to
					 * follow the GUI state... */
					width  = fmt.fmt.pix.width = res[cres].width;
					height = fmt.fmt.pix.height = res[cres].height;
					if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)){
						perror("VIDIOC_S_FMT");
						return;
					}
					printf("change_fmt = %d\n", change_fmt);
					enumerate_res();
					init_display();
					change_fmt = 0;
					/* Refresh the resolution list in the GUI */
					reset_res = 1;
				}
				/* Change framerate */
				if(change_fps) {
					set_v4l2_framerate(change_fps);
					change_fps = 0;
				}
				/* Start/stop video capture */
				if (start_capture) {
					capture_state = CAPTURE_STARTED;
					/* Restart the decoder too so
					 * that we don't keep old states
					 * and decoding delays */
					init_display();
					window_show(win);
					start_capture=0;
				}
				else if (stop_capture) {
					capture_state = CAPTURE_STOPPED;
					window_hide(win);
					lastcc = -1;
					stop_capture=0;
				}

				init_mmap();
				if (capture_state == CAPTURE_STARTED)
					start_capturing();

				/* Refresh the framerate in the GUI */
				reset_fr = 1;
			}

			if (capture_state == CAPTURE_STARTED) {
				if (read_frame())
					break;
			}
			/* EAGAIN - continue select loop. */
		}
	}
}
static void usage(FILE *fp, int argc, char **argv)
{
	fprintf(fp,
		 "Usage: %s [options]\n\n"
		 "Version %s\n"
		 "Options:\n"
		 "-v | --version       Display version\n"
		 "-d | --device name   Video device name [%s]\n"
		 "-h | --help          Print this message\n"
		 "-o | --output        Outputs video stream to given filename\n"
		 "-f | --format        Fourcc code to choose pixel format\n"
		 "-c | --count         Number of frames to grab [%i]\n"
		 "-s | --stop          Start with video capture stopped\n"
#ifndef NOLUA
		 "-l | --lua filename  Lua script to run tests\n"
#endif
		 "-b | --buffer        Number of buffer to allocate for video [%i]\n"
		 "-t | --time          Quit after the specified number of seconds [%i]\n"
#ifndef NOAUDIO
		 "--aout  filename     Outputs audio stream to given filename\n"
		 "--adev  name         Audio device name [%s]\n"
#endif
#ifndef NOX
		 "--fullscreen         Start the video in fullscreen\n"
		 "--nogui              Do not start the control GUI\n"
#endif
		 "",
		 argv[0],
		 MXVIEW_VERSION,
		 dev_name,
		 frame_count,
		 buffer_count,
		 time_count
#ifndef NOAUDIO
		 ,alsa_device_capture
#endif
		 );
}

static const char short_options[] = "vd:ho:f:c:sl:b:t:";

#define AOUT        1
#define ADEV        2
#define FULLSCREEN  3
#define NOGUI       4
static const struct option
long_options[] = {
	{ "version",    no_argument,       NULL, 'v' },
	{ "device",     required_argument, NULL, 'd' },
	{ "help",       no_argument,       NULL, 'h' },
	{ "output",     required_argument, NULL, 'o' },
	{ "format",     required_argument, NULL, 'f' },
	{ "count",      required_argument, NULL, 'c' },
	{ "stop",       no_argument,       NULL, 's' },
	{ "lua",        no_argument,       NULL, 'l' },
	{ "buffer",     no_argument,       NULL, 'b' },
	{ "timer",      required_argument, NULL, 't' },
	{ "aout ",      required_argument, NULL, AOUT },
	{ "adev ",      required_argument, NULL, ADEV },
	{ "fullscreen", no_argument,       NULL, FULLSCREEN },
	{ "nogui",      no_argument,       NULL, NOGUI },
	{ 0, 0, 0, 0 }
};

int main(int argc, char **argv)
{
	int lua_enable = 0;
	char * lua_arg = 0;

	dev_name = "/dev/video0";

	for (;;) {
		int idx;
		int c;

		c = getopt_long(argc, argv,
				short_options, long_options, &idx);

		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case 'v':
			printf("%s\n", MXVIEW_VERSION);
			exit(EXIT_SUCCESS);
			break;

		case 'd':
			dev_name = optarg;
			break;

		case 'h':
			usage(stdout, argc, argv);
			exit(EXIT_SUCCESS);

		case 'o':
			out_buf++;
			filename_video = optarg;
			break;

		case 'f':
			if(strlen(optarg)!=4) {
				fprintf(stderr, "FourCC must be 4 bytes.\n");
				exit(1);
			}
			force_format++;
			fourcc = v4l2_fourcc(optarg[0], optarg[1],
					optarg[2], optarg[3]);
			break;

		case 'c':
			errno = 0;
			frame_count = strtol(optarg, NULL, 0);
			if (errno)
				errno_exit(optarg);
			break;

		case 's':
			capture_state = CAPTURE_STOPPED;
			audio_state = AUDIO_STOPPED;
			break;

		case 'l':
			lua_enable = 1;
			lua_arg = optarg;
			break;

		case 'b':
			errno = 0;
			buffer_count = strtol(optarg, NULL, 0);
			if (errno)
				errno_exit(optarg);
			break;

		case 't':
			errno = 0;
			time_count = strtol(optarg, NULL, 0);
			if (errno)
				errno_exit(optarg);
			break;

		case AOUT:
			filename_audio = optarg;
			audio_state = AUDIO_STARTED;
			break;

		case ADEV:
			alsa_device_capture = optarg;
			break;

		case FULLSCREEN:
			start_in_fullscreen = 1;
			break;

		case NOGUI:
			nogui = 1;
			break;

		default:
			usage(stderr, argc, argv);
			exit(EXIT_FAILURE);
		}
	}

	/* Initialize device */
	open_device();
	init_device();
	enumerate_formats();
	enumerate_res();
	init_display();

#ifndef NOLUA
	/* start lua in a separate thread */
	if (lua_enable != 0)
		lua_init(lua_arg);
#endif

#ifndef NOAUDIO
	/* start the audio capture in a separate thread */
	if (audio_state == AUDIO_STARTED)
		start_audio((char *) alsa_device_capture);
#endif

#ifndef NOX
	if (!nogui) {
		/* Initialize GUI */
		gui_init( &argc, &argv );
	}
#endif

	/* Start capturing the stream */
	if (capture_state == CAPTURE_STARTED) {
		start_capturing();
#ifndef NOX
		window_show(win);
		if(start_in_fullscreen)
			window_toggle_fullscreen(win);
#endif
	}

	mainloop();

	/* Exit properly */
	stop_capturing();
	uninit_mmap();
	close_device();
	fprintf(stderr, "\n");
	return 0;
}
