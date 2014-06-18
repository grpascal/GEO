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

#ifndef _QBOX_H
#define _QBOX_H

int qbox_parse_header(uint8_t *buf, int *channel_id, video_format_t *fmt, uint8_t **data_buf, uint32_t *size, uint64_t *ts);
int audio_param_parser(audio_params_t *h, unsigned char *buf, int len);
int get_qbox_hdr_size(void);
#endif	/* _QBOX_H */
