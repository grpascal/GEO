/*******************************************************************************
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Maxim Integrated Products.  It is subject to the terms of a
 * License Agreement between Licensee and Maxim Integrated Products.
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and
 * any derivative work shall retain this copyright notice.
 *
 * Copyright (c) 2012 Maxim Integrated Products.
 * All rights reserved.
 *
 ******************************************************************************/

#include <stdio.h>
#include <mxuvc.h>
#include "common.h"
#include "qbox.h" 
#include "qmedutil.h"
#include "qmed.h"
#include "qboxutil.h"

#define QBOX_TYPE	(('q' << 24) | ('b' << 16) | ('o' << 8) | ('x'))

#if __BYTE_ORDER == LITTLE_ENDIAN
#define be32_to_cpu(x)	((((x) >> 24) & 0xff) | \
			 (((x) >> 8) & 0xff00) | \
			 (((x) << 8) & 0xff0000) | \
			 (((x) << 24) & 0xff000000))
#define be16_to_cpu(x)	((((x) >> 8) & 0xff) | \
			 (((x) << 8) & 0xff00))
#else
#define be32_to_cpu(x)
#define be24_to_cpu(x)
#define be16_to_cpu(x)
#endif

#define QBOX_SAMPLE_TYPE_AAC 0x1
#define QBOX_SAMPLE_TYPE_QAC 0x1
#define QBOX_SAMPLE_TYPE_H264 0x2
#define QBOX_SAMPLE_TYPE_QPCM 0x3
#define QBOX_SAMPLE_TYPE_DEBUG 0x4
#define QBOX_SAMPLE_TYPE_H264_SLICE 0x5
#define QBOX_SAMPLE_TYPE_QMA 0x6
#define QBOX_SAMPLE_TYPE_VIN_STATS_GLOBAL 0x7
#define QBOX_SAMPLE_TYPE_VIN_STATS_MB 0x8
#define QBOX_SAMPLE_TYPE_Q711 0x9
#define QBOX_SAMPLE_TYPE_Q722 0xa
#define QBOX_SAMPLE_TYPE_Q726 0xb
#define QBOX_SAMPLE_TYPE_Q728 0xc
#define QBOX_SAMPLE_TYPE_JPEG 0xd
#define QBOX_SAMPLE_TYPE_MPEG2_ELEMENTARY 0xe
#define QBOX_SAMPLE_TYPE_USER_METADATA 0xf
#define QBOX_SAMPLE_TYPE_MB_STAT_LUMA 0x10
#define QBOX_SAMPLE_TYPE_MAX 0x11

//#define QBOX_SAMPLE_FLAGS_CONFIGURATION_INFO 0x0001
#define QBOX_SAMPLE_FLAGS_CTS_PRESENT        0x0002
//#define QBOX_SAMPLE_FLAGS_SYNC_POINT         0x0004
#define QBOX_SAMPLE_FLAGS_DISPOSABLE         0x0008
#define QBOX_SAMPLE_FLAGS_MUTE               0x0010
#define QBOX_SAMPLE_FLAGS_BASE_CTS_INCREMENT 0x0020
#define QBOX_SAMPLE_FLAGS_META_INFO          0x0040
#define QBOX_SAMPLE_FLAGS_END_OF_SEQUENCE    0x0080
#define QBOX_SAMPLE_FLAGS_END_OF_STREAM      0x0100
#define QBOX_SAMPLE_FLAGS_QMED_PRESENT       0x0200
#define QBOX_SAMPLE_FLAGS_PKT_HEADER_LOSS    0x0400
#define QBOX_SAMPLE_FLAGS_PKT_LOSS           0x0800
#define QBOX_SAMPLE_FLAGS_120HZ_CLOCK        0x1000
#define QBOX_SAMPLE_FLAGS_TS                 0x2000
#define QBOX_SAMPLE_FLAGS_TS_FRAME_START     0x4000
#define QBOX_SAMPLE_FLAGS_PADDING_MASK       0xFF000000

#define QBOX_VERSION(box_flags)                                         \
    ((box_flags) >> 24)
#define QBOX_BOXFLAGS(box_flags)                                        \
    (((box_flags) << 8) >> 8)
#define QBOX_FLAGS(v, f)                                                \
    (((v) << 24) | (f))
#define QBOX_SAMPLE_PADDING(sample_flags)                               \
    (((sample_flags) & QBOX_SAMPLE_FLAGS_PADDING_MASK) >> 24)
#define QBOX_SAMPLE_FLAGS_PADDING(sample_flags, padding)                \
    ((sample_flags) | ((padding) << 24))

typedef struct
{
    unsigned long v : 8;
    unsigned long f : 24;
} QBoxFlag;

typedef struct
{
    unsigned long samplerate;
    unsigned long samplesize;
    unsigned long channels;
} QBoxMetaA;

typedef struct
{
    unsigned long width;
    unsigned long height;
    unsigned long gop;
    unsigned long frameticks;
} QBoxMetaV;

typedef union
{
    QBoxMetaA a;
    QBoxMetaV v;
} QBoxMeta;

typedef struct
{
    unsigned long addr;
    unsigned long size;
} QBoxSample;

//////////////////////////////////////////////////////////////////////////////
// version: 1
//
// 64 bits cts support

typedef struct
{
    unsigned long ctslow;
    unsigned long addr;
    unsigned long size;
} QBoxSample1;


struct qbox_sample {
	uint32_t addr;
	uint32_t size;
};

struct qbox_sample_v1 {
	uint32_t ctslow;
	uint32_t addr;
	uint32_t size;
};

typedef struct qbox_header {
	uint32_t box_size;
	uint32_t box_type;
	/* TODO test on BE and LE machines */
	struct {
		uint32_t version : 8;
		uint32_t flags   : 24;
	} box_flags;
	uint16_t stream_type;
	uint16_t stream_id;
	uint32_t sample_flags;
	uint32_t sample_cts;
	uint32_t sample_cts_low;
}__attribute__((__packed__)) qbox_header_t;

/* QBOX header parser
 * buf: data buffer passed by application
 * channel_id: channel id
 * fmt: data format of the channel
 * data_buf: data starts here
 * size: size of data
 * ts: timestamp; only 32 bit timestamps are passed to application
 * Returns 0 if QBOX header was parsed successfully
 * Returns 1 if the frame is not a valid qbox frame
 */
int qbox_parse_header(uint8_t *buf, int *channel_id, video_format_t *fmt, uint8_t **data_buf, uint32_t *size, uint64_t *ts)
{
	qbox_header_t *qbox = (qbox_header_t*)buf;
	int hdr_len;
	uint32_t flags;

	if (be32_to_cpu(qbox->box_type) != QBOX_TYPE) {
		return 1;
	}

	hdr_len = sizeof(qbox_header_t);
	if (qbox->box_flags.version == 1) {
		*ts = ((uint64_t)be32_to_cpu(qbox->sample_cts) << 32) | be32_to_cpu(qbox->sample_cts_low);
	} else {
		*ts = be32_to_cpu(qbox->sample_cts);
		hdr_len -= sizeof(uint32_t);
	}

	//map qbox header type to video_format_t type before passing to user
	switch(be16_to_cpu(qbox->stream_type)){
		case QBOX_SAMPLE_TYPE_AAC:
			*fmt = VID_FORMAT_H264_AAC_TS;
			break;
		case QBOX_SAMPLE_TYPE_H264:
			flags = be32_to_cpu(qbox->sample_flags);
			if (0 != (flags & QBOX_SAMPLE_FLAGS_TS)) {
				*fmt = VID_FORMAT_H264_TS;
			} else
				*fmt = VID_FORMAT_H264_RAW;
			break;
		case QBOX_SAMPLE_TYPE_JPEG:
			*fmt = VID_FORMAT_MJPEG_RAW;
			break;
		case QBOX_SAMPLE_TYPE_MB_STAT_LUMA:
			*fmt = VID_FORMAT_GREY_RAW;
			break;
		default:
			TRACE("Wrong Qbox format\n");
			break;
	}
	*channel_id = be16_to_cpu(qbox->stream_id);
	//*fmt = be16_to_cpu(qbox->stream_type);
	*data_buf = ((uint8_t*)buf + hdr_len);
	*size = be32_to_cpu(qbox->box_size) - hdr_len;

	return 0;
}


//qmed functions
#define GetExtPtr(pHdr)  \
    ((GetQMedBaseVersion(pHdr) == 1) ? (pHdr + sizeof(QMedStruct) + sizeof(QMedVer1InfoStruct)) : \
                                      (pHdr + sizeof(QMedStruct)))

static const int SampleFreq[12] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000
};

unsigned int GetQMedAACAudioSpecificConfigSize(unsigned char *pQMed)
{
    QMedAACStruct *pQMedAAC = (QMedAACStruct *) GetExtPtr(pQMed);
    
    return BE32(pQMedAAC->audioSpecificConfigSize);
}

unsigned long GetQMedBaseVersion(unsigned char *pQMed)
{
    QMedStruct *pQMedBase = (QMedStruct *) pQMed;
    
    return BE8(pQMedBase->boxFlags.field.v);
}

unsigned long GetQMedAACHeaderSize(int baseVersion, int aacVersion)
{
    if (baseVersion == 0)
        return (sizeof(QMedStruct) + sizeof(QMedAACStruct));
    else
        return (sizeof(QMedStruct) + sizeof(QMedVer1InfoStruct) + sizeof(QMedAACStruct));
}

unsigned int GetQMedAACSampleFrequency(unsigned char *pQMed)
{
    QMedAACStruct *pQMedAAC = (QMedAACStruct *) GetExtPtr(pQMed);
    
    return BE32(pQMedAAC->samplingFrequency);
}

unsigned int GetQMedAACChannels(unsigned char *pQMed)
{
    QMedAACStruct *pQMedAAC = (QMedAACStruct *) GetExtPtr(pQMed);
    
    return BE32(pQMedAAC->channels);
}

unsigned long GetQMedBaseBoxSize(unsigned char *pQMed)
{
    QMedStruct *pQMedBase = (QMedStruct *) pQMed;
    
    return BE32(pQMedBase->boxSize);
}

int get_qbox_hdr_size(void)
{
	return sizeof(qbox_header_t);
}

int audio_param_parser(audio_params_t *h, unsigned char *buf, int len)
{
    unsigned char *p = buf;
    // params for audiospecific config
    unsigned char *asc;
    int ascSize;
    unsigned char *pQMedAAC = NULL;	
    int hdr_len;

    qbox_header_t *qbox = (qbox_header_t *)buf;

    if (be32_to_cpu(qbox->box_type) != QBOX_TYPE) {
        TRACE("ERR: Wrong mux format\n");
        return 1;
    }

    hdr_len = sizeof(qbox_header_t);

    if (qbox->box_flags.version == 1) {
        h->timestamp = be32_to_cpu(qbox->sample_cts_low);
    } else {
        h->timestamp = be32_to_cpu(qbox->sample_cts);
        hdr_len -= sizeof(uint32_t);
    }

    h->audiocodectype = be16_to_cpu(qbox->stream_type);
    if(be16_to_cpu(qbox->stream_type) == QBOX_SAMPLE_TYPE_AAC)
    {

        // skip the qbox header
        p += hdr_len;
        len -= hdr_len;

        // get the media description box to build the adts header
        if (be32_to_cpu(qbox->sample_flags) & QBOX_SAMPLE_FLAGS_QMED_PRESENT)
        {
            pQMedAAC = (unsigned char *) p;

            // retrieve the audio specific config size
            ascSize = GetQMedAACAudioSpecificConfigSize(pQMedAAC);

            if (!ascSize)
            {
                TRACE("ERROR !!! No ASC found\n");
                return 1;
            }

            asc = (unsigned char *) (p + GetQMedAACHeaderSize(GetQMedBaseVersion(pQMedAAC), 0));


            h->samplefreq = GetQMedAACSampleFrequency(pQMedAAC);
            h->channelno = GetQMedAACChannels(pQMedAAC);
            // skip past the media box
            p += GetQMedBaseBoxSize(pQMedAAC);
            len -= GetQMedBaseBoxSize(pQMedAAC);


            h->audioobjtype = asc[0] >> 3;

            // support 1 or 2 ch only
            if ((h->channelno < 1) ||
                (h->channelno > 2))
            {
                TRACE("Channel number unsupported %d", h->channelno);
            }

        }

        h->dataptr = p;
        h->framesize = len;
    }

    else
    {
        TRACE("Unsupported Audio Codec \n");
        return 1;
    }


	return 0;
}
