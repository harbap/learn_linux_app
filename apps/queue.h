/******************************************************************************
**  Copyright Danlow Xichen Technologies, Inc.
**
** FILE NAME
**    queue.c
**
** DESCRIPTION
**    This file contains queue applications.
**
** History:
**-----------------------------------------------------------------------------
**  Ver     Date           Author         Description
**-----------------------------------------------------------------------------
**  1.0  2020-08-14       huangsl      The first release
*******************************************************************************/


#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define CFG_MULTI_THREAD				1

typedef struct{
	unsigned char *pBuffer;
	unsigned int bufSize;
	unsigned int head;
	unsigned int tail;
#if defined(CFG_MULTI_THREAD)	
	pthread_mutex_t semaphore;
#endif
}S_QUEUE;

int QueueInit(S_QUEUE *pQueue,void *pBuffer,unsigned int bufferSize);
int QueueDeinit(S_QUEUE *pQueue);
int QueueLen(S_QUEUE *pQueue);
int QueuePut(S_QUEUE *pQueue,unsigned char *data,unsigned int size);
int QueueGet(S_QUEUE *pQueue,unsigned char *data,unsigned int size);
int QueueClean(S_QUEUE *pQueue);

#endif /*_QUEUE_H_*/

