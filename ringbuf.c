/*****************************************************************************
*  ringbuf.c -- Ring buffer routines 
*
*  Author: Jimmy Blair
*  
*****************************************************************************/
#include <linux/module.h>
#include "linux/stddef.h"
#include "ringbuf.h"

void ringbuf_init(RINGBUF * pRing, void **ppArray, int len, int full)
{
	pRing->ppArray = ppArray;
	pRing->iRead = 0;
	pRing->iLen = len;
	if (full)
		pRing->iWrite = pRing->iLen - 1;
	else
		pRing->iWrite = 0;
}

void *ringbuf_get(RINGBUF * pRing)
{
	int next;
	void *pData;

	if (pRing->iRead == pRing->iWrite)
		return (NULL);	/* Ring empty */

	pData = pRing->ppArray[pRing->iRead];

	pRing->iRead = (pRing->iRead + 1) % pRing->iLen;

	return (pData);
}

void *ringbuf_put(RINGBUF * pRing, void *pData)
{
	int next = (pRing->iWrite + 1) % pRing->iLen;

	if (next == pRing->iRead)
		return (NULL);	/* Ring full */

	pRing->ppArray[pRing->iWrite] = pData;
	pRing->iWrite = next;

	return (pData);
}

EXPORT_SYMBOL (ringbuf_init);
EXPORT_SYMBOL (ringbuf_get);
EXPORT_SYMBOL (ringbuf_put);