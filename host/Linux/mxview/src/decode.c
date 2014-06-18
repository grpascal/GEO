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

#include <libavcodec/avcodec.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <jpegutils.h>
#include <mjpeg_logging.h>

#include "decode.h"

struct decode {
	char		type;
	/* AVC */
	AVCodecContext 	*codec_ctx;
	AVFrame 	*frame;
	/* JPEG */
};

/* AVC */
static struct decode* decode_create_avc()
{
	struct decode *dec;
	AVCodec *av_codec;

	dec = malloc(sizeof(struct decode));
	if (dec == NULL) {
		perror("malloc");
		goto err_decode;
	}
	dec->type = DEC_AVC;

	avcodec_init();
	avcodec_register_all();

	av_codec = avcodec_find_decoder(CODEC_ID_H264);

	if(av_codec == NULL) {
		printf("avcodec_find_decoder failed\n");
		goto err_decode;
	}

	dec->codec_ctx = avcodec_alloc_context();
	if (dec->codec_ctx == NULL) {
		printf("avcodec_alloc_context failed\n");
		goto err_decode;
	}
	//dec->codec_ctx->debug = 0xffff;
	if(avcodec_open(dec->codec_ctx, av_codec)<0) {
		printf("avcodec_open failed\n");
		goto err_decode;
	}

	dec->frame = avcodec_alloc_frame();
	if (dec->frame == NULL) {
		printf("avcodec_alloc_frame failed\n");
		goto err_decode;
	}

	return dec;

err_decode:
	if (dec != NULL)
		free(dec);
	return NULL;

}

static void decode_destroy_avc(struct decode* dec)
{
	av_free(dec->frame);
	av_free(dec->codec_ctx);
	free(dec);
}

static void avcodec_copy_out(AVCodecContext* ctx, AVFrame* frame, struct framebuffer* frm)
{
	int z;
	int plane_y_idx = 0;
	int plane_u_idx = 1;
	int plane_v_idx = 2;
	/* Defaults for YUV 420 */
	int plane_u_inc = 1;
	int plane_v_inc = 1;
	int plane_u_fac = 2;
	int plane_v_fac = 2;
	void *data;
	uint8_t *dest;

	/* setup framebuffer */
	frm->pixelformat = FOURCC_I420;
	frm->width = ctx->width;
	frm->height = ctx->height;
	frm->size = 3*frm->width*frm->height/2;
	frm->data = malloc(frm->size);

	dest = frm->data;

	if(ctx->pix_fmt == PIX_FMT_YUV422P || ctx->pix_fmt == PIX_FMT_YUVJ422P) {
		plane_u_fac = plane_v_fac = 1;
		plane_u_inc = plane_v_inc = 2;
	}

	for (z = 0; frame->data[plane_y_idx] && z < ctx->height; z++)
	{
		data = (void*)&(frame->data[plane_y_idx][frame->linesize[plane_y_idx]*z]);
		memcpy(dest, data, ctx->width);
		dest += ctx->width;
	}
	for (z = 0; frame->data[plane_u_idx] && z < ctx->height/plane_u_fac; z+=plane_u_inc)
	{
		data = (void*)&(frame->data[plane_u_idx][frame->linesize[plane_u_idx]*z]);
		memcpy(dest, data, ctx->width / 2);
		dest += ctx->width / 2;
	}
	for (z = 0; frame->data[plane_v_idx] && z < ctx->height/plane_v_fac; z+=plane_v_inc)
	{
		data = (void*)&(frame->data[plane_v_idx][frame->linesize[plane_v_idx]*z]);
		memcpy(dest, data, ctx->width / 2);
		dest += ctx->width / 2;
	}
}

static struct framebuffer* decode_frame_avc(struct decode *dec, const unsigned char *frame, signed long length)
{
	AVPacket avpkt;
	signed long decoded_len;
	int got_pic = 0;
	avpkt.pts = AV_NOPTS_VALUE;
	avpkt.dts = AV_NOPTS_VALUE;
	avpkt.data = (uint8_t*)frame;
	avpkt.size = length;
	avpkt.duration = 0;

	decoded_len = avcodec_decode_video2(dec->codec_ctx, dec->frame, &got_pic, &avpkt);

	if (decoded_len < 0) {
		printf("avcodec_decode_video failed: res: %ld, gotPicture: %d\n",
				decoded_len, got_pic);
		return NULL;
	}
	if (!got_pic && decoded_len != length) {
		printf("decoded less data: given %ld, decoded %ld\n",
				length, decoded_len);
	}
	if (got_pic) {
		struct framebuffer* frm = malloc(sizeof(struct framebuffer));
		avcodec_copy_out(dec->codec_ctx, dec->frame, frm);
		return frm;
	}
	return NULL;
}


/* JPEG */
static struct decode* decode_create_jpeg()
{
	struct decode *dec;
	dec = malloc(sizeof(struct decode));
	if (dec == NULL) {
		perror("malloc");
		return NULL;
	}
	dec->type = DEC_MJPEG;

	mjpeg_default_handler_verbosity(0);

	return dec;
}

static void decode_destroy_jpeg(struct decode* dec)
{
	free(dec);
}

static struct framebuffer* decode_frame_jpeg(struct decode* dec, const unsigned char *frame, signed long length)
{
	struct framebuffer* frm = malloc(sizeof(struct framebuffer));
	/* Width and height are set to 0 to indicate that we don't
	 * want to force the output picture width and height */
	frm->pixelformat = FOURCC_I420;
	frm->width = 0;
	frm->height = 0;
	frm->data = NULL;
	frm->size = 0;

	int ret = decode_jpeg_raw(frame, length, Y4M_ILACE_TOP_FIRST,
			Y4M_CHROMA_420JPEG, frm);
	if (ret < 0) {
		if(frm->data)
			free(frm->data);
		free(frm);
		printf("decode_jpeg_raw error\n");
		return NULL;
	}
	return frm;
}

/* Generic */
struct decode* decode_create(char type)
{
	switch (type) {
	case DEC_AVC:
		return decode_create_avc();
	case DEC_MJPEG:
		return decode_create_jpeg();
	default:
		printf("decode_create error: bad decode type\n");
		return NULL;
	}
}

void decode_destroy(struct decode* dec)
{
	switch (dec->type) {
	case DEC_AVC:
		decode_destroy_avc(dec);
		break;
	case DEC_MJPEG:
		decode_destroy_jpeg(dec);
		break;
	default:
		printf("decode_destroy error: bad decode type\n");
	}
}

struct framebuffer* decode_frame(struct decode* dec, const unsigned char *frame, signed long length)
{
	switch (dec->type) {
	case DEC_AVC:
		return decode_frame_avc(dec, frame, length);
	case DEC_MJPEG:
		return decode_frame_jpeg(dec, frame, length);
	default:
		printf("decode_frame error: bad decode type\n");
		return NULL;
	}
}
