/* 
 * 
 * Copyright 2007 MicroMemory, LLC.
 * Phil Terry <pterry@micromemory.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef RDDMA_H
#define RDDMA_H

#include <linux/kobject.h>
#include <stdarg.h>

#define CONFIG_RDDMA_DEBUG

/* Merge DMA descriptors for adjacent pages.
 * This should always be on, unless you're trying
 * to generate long DMA chains for testing.
 */
#define OPTIMIZE_DESCRIPTORS

#if 0
/* Individually queue binds to DMA engine.
 * This allows multi-channel DMA controllers to
 * run binds in parallel.
 */
#define PARALLELIZE_BIND_PROCESSING
#else
/* Link all binds in a transfer into a single
 * DMA chain. DMA is parallelized on multi-channel
 * controllers, but at the transfer level.
 */
#define SERIALIZE_BIND_PROCESSING
#endif

extern unsigned int rddma_debug_level;
/* Debug level is treated as a four-bit level integer, when, and a 28-bit mask, who_what */
#define RDDMA_DBG_WHEN 0xf
#define RDDMA_DBG_WHO_WHAT ~RDDMA_DBG_WHEN

/* The when level follows syslog conventions: */
#define	RDDMA_DBG_EMERG	        0	/* system is unusable			*/
#define	RDDMA_DBG_ALERT	        1	/* action must be taken immediately	*/
#define	RDDMA_DBG_CRIT	        2	/* critical conditions			*/
#define	RDDMA_DBG_ERR	        3	/* error conditions			*/
#define	RDDMA_DBG_WARNING	4	/* warning conditions			*/
#define	RDDMA_DBG_NOTICE	5	/* normal but significant condition	*/
#define	RDDMA_DBG_INFO	        6	/* informational			*/
#define	RDDMA_DBG_DEBUG	        7	/* debug-level messages			*/

/* The top-bits of who_what select who, a susbsystem, component, type etc. */
#define RDDMA_DBG_LOCATION 0x80000000
#define RDDMA_DBG_SMB      0x40000000
#define RDDMA_DBG_XFER     0x20000000
#define RDDMA_DBG_CDEV     0x10000000
#define RDDMA_DBG_FABRIC   0x08000000
#define RDDMA_DBG_FABNET   0x04000000
#define RDDMA_DBG_OPS      0x02000000
#define RDDMA_DBG_LOCOPS   0x01000000
#define RDDMA_DBG_FABOPS   0x00800000
#define RDDMA_DBG_PARSE    0x00400000
#define RDDMA_DBG_DMA      0x00200000
#define RDDMA_DBG_DMARIO   0x00100000
#define RDDMA_DBG_SUBSYS   0x00080000
#define RDDMA_DBG_BIND     0x00040000
#define RDDMA_DBG_DST      0x00020000
#define RDDMA_DBG_SRC      0x00010000
#define RDDMA_DBG_MMAP     0x00008000

#define RDDMA_DBG_WHO      0xffff8000

/* The bottom bits of who_what select a functional area, what to debug */
#define RDDMA_DBG_FUNCALL  0x00000010
#define RDDMA_DBG_LIFE     0x00000020
#define RDDMA_DBG_DMA_CHAIN   0x00000040

#define RDDMA_DBG_WHAT     (~RDDMA_DBG_WHO & RDDMA_DBG_WHO_WHAT)

/* So from msb to lsb debug_level can be thought of as defining who, what, when */
/* eg Location operators, function calls, when info is wanted */
/* RDDMA_DBG_LOCATION | RDDMA_DBG_FUNCALL | RDDMA_DBG_INFO */
#define RDDMA_DBG_EVERYONE   RDDMA_DBG_WHO
#define RDDMA_DBG_EVERYTHING RDDMA_DBG_WHAT
#define RDDMA_DBG_ALWAYS     RDDMA_DBG_WHEN
#define RDDMA_DBG_ALL       (RDDMA_DBG_EVERYONE | RDDMA_DBG_EVERYTHING | RDDMA_DBG_ALWAYS)

#define RDDMA_DBG_SYSLOG_LEVEL "<1>"
#ifdef CONFIG_RDDMA_DEBUG

static void rddma_debug(char *format, ...) __attribute__((unused,format(printf,1,2)));
static void rddma_debug(char *format, ...)
{
	va_list args;
	va_start(args,format);
	vprintk(format,args);
	va_end(args);
}
#define RDDMA_ASSERT(c,f,arg...) if (!(c)) rddma_debug(RDDMA_DBG_SYSLOG_LEVEL f,## arg)
#define RDDMA_DEBUG(l,f, arg...) if ((((l) & RDDMA_DBG_WHEN) <= (rddma_debug_level & RDDMA_DBG_WHEN)) && \
				     ((((l) & RDDMA_DBG_WHO_WHAT) & rddma_debug_level ) == ((l) & RDDMA_DBG_WHO_WHAT)) ) rddma_debug(RDDMA_DBG_SYSLOG_LEVEL f, ## arg)
#define RDDMA_DEBUG_SAFE(l,c,f,arg...) if ((c)) RDDMA_DEBUG((l),f, ## arg)
#else
#define RDDMA_ASSERT(c,f,arg...) do {} while (0)
#define RDDMA_DEBUG(l,f,arg...) do {} while (0)
#define RDDMA_DEBUG_SAFE(l,c,f,arg...) do {} while (0)
#endif

/* Helper for printing sysfs attributes... define your show function (...char *buffer...) with {int left = PAGE_SIZE;int size = 0; .... return size;} */
/* then ... can include ATTR_PRINTF("format string",...); */
#define ATTR_PRINTF(f,arg...) size += snprintf(buffer+size,left,f, ## arg); left -=size
#define MACADDRFMT "%x:%x:%x:%x:%x:%x"
#define MACADDRBYTES(mac) mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]

static inline struct kobject *to_kobj(struct list_head *entry)
{
    return entry ? container_of(entry, struct kobject, entry) : NULL;
}

#endif /* RDDMA_H */
