/*******************************************************************************
*  ringbuf.h 
*
*  Author: Jimmy Blair, Upcall Software Consulting
*******************************************************************************/

#ifndef _RINGBUF_H
#define _RINGBUF_H

typedef struct _ringbuffer {
    void ** ppArray;
    int iRead;          /* points at next location to read */
    int iWrite;         /* points at next location to write */
    int iLen;           /* length of ring buffer */
}RINGBUF;

#define RING_FULL 1
#define RING_EMPTY 0

/*
*  The array of pointers must be 1 element larger than the 'len'
*  of the ringbuffer.  
*/

extern void ringbuf_init (RINGBUF * pRing, void ** ppArray, int len, int full);
extern void * ringbuf_get (RINGBUF * pRing);
extern void * ringbuf_put (RINGBUF * pRing, void * pData);

#endif /* _RINGBUF_H */
