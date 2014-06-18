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

struct queue;

int queue_init(struct queue **q, char *name, int size, int item_size, int threshold);
int queue_free(struct queue *q);
int queue_reset(struct queue *q);
int queue_is_prefilled(struct queue *q);
int queue_stash(struct queue *q, void *p);
int queue_pop(struct queue *q, void *p);
int queue_getsize(struct queue *q);
