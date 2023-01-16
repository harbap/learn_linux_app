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
#include "queue.h"
#include "stdio.h"

int QueueInit(S_QUEUE *pQueue,void *pBuffer,unsigned int bufferSize)
{
	if(pQueue == NULL)
		return -1;
	memset(pBuffer,0,bufferSize);
	pQueue->bufSize = bufferSize;
	pQueue->pBuffer = pBuffer;
	pQueue->head = 0;
	pQueue->tail = 0;
	printf("pQueue->buf:%x pQueue->bufSize:%d\n",pQueue->pBuffer,pQueue->bufSize);
#if (CFG_MULTI_THREAD)
	pthread_mutex_init(&pQueue->semaphore,NULL);
#endif
	return 0;
}

int QueueDeinit(S_QUEUE *pQueue)
{
#if(CFG_MULTI_THREAD)
	pthread_mutex_lock(&pQueue->semaphore);
#endif
	pQueue->head = 0;
	pQueue->tail = 0;
#if (CFG_MULTI_THREAD)
	pthread_mutex_unlock(&pQueue->semaphore);
	pthread_mutex_destroy(&pQueue->semaphore);
#endif
	return 0;
}
int QueueLen(S_QUEUE *pQueue)
{
	int len;
	if(pQueue == NULL)
		return 0;
#if (CFG_MULTI_THREAD)
	pthread_mutex_lock(&pQueue->semaphore);
#endif
	if(pQueue->tail >= pQueue->head)
		len = pQueue->tail - pQueue->head;
	else
		len = pQueue->bufSize + pQueue->tail - pQueue->head;
#if (CFG_MULTI_THREAD)
	pthread_mutex_unlock(&pQueue->semaphore);
#endif
	return len;
}

int QueuePut(S_QUEUE *pQueue,unsigned char *data,unsigned int size)
{
	int ret = 0,i = 0;

	if(pQueue == NULL || data == NULL)
		return ret;
#if (CFG_MULTI_THREAD)
	pthread_mutex_lock(&pQueue->semaphore);
#endif
	for(i = 0;i < size;i++){
		if(pQueue->head != (pQueue->tail + 1) % pQueue->bufSize){
			pQueue->pBuffer[pQueue->tail] = data[i];
			pQueue->tail = (pQueue->tail + 1) % pQueue->bufSize;
			ret ++;
		}
	}
#if (CFG_MULTI_THREAD)
	pthread_mutex_unlock(&pQueue->semaphore);
#endif
	return ret;
}

int QueueGet(S_QUEUE *pQueue,unsigned char *data,unsigned int size)
{
	int GetSize = size;
	unsigned char *pGetBuf = data;
	
	if(pQueue == NULL || data == NULL)
		return 0;
#if (CFG_MULTI_THREAD)
	pthread_mutex_lock(&pQueue->semaphore);
#endif
	while(pQueue->head != pQueue->tail){
		if(GetSize != 0){
			*pGetBuf++ = pQueue->pBuffer[pQueue->head];
			pQueue->head = (pQueue->head + 1) % pQueue->bufSize;
			GetSize --;
		}else{
			break;
		}
	}
#if (CFG_MULTI_THREAD)
	pthread_mutex_unlock(&pQueue->semaphore);
#endif

	return (size - GetSize);
}
int32_t QueueClean(S_QUEUE *pQueue)
{
#if (CFG_MULTI_THREAD)
	pthread_mutex_lock(&pQueue->semaphore);
#endif	
	pQueue->head = pQueue->tail;
#if (CFG_MULTI_THREAD)
	pthread_mutex_unlock(&pQueue->semaphore);
#endif
	return 0;
}

