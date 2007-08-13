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

extern int rddma_debug_level;
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

#define RDDMA_DBG_WHO      0xffc00000

/* The bottom bits of who_what select a functional area, what to debug */
#define RDDMA_DBG_FUNCALL  0x00000010
#define RDDMA_DBG_LIFE     0x00000020

#define RDDMA_DBG_WHAT     (~RDDMA_DBG_WHO & RDDMA_DBG_WHO_WHAT)

/* So from msb to lsb debug_level can be thought of as defining who, what, when */
/* eg Location operators, function calls, when info is wanted */
/* RDDMA_DBG_LOCATION | RDDMA_DBG_FUNCALL | RDDMA_DBG_INFO */
#define RDDMA_DBG_EVERYONE   RDDMA_DBG_WHO
#define RDDMA_DBG_EVERYTHING RDDMA_DBG_WHAT
#define RDDMA_DBG_ALWAYS     RDDMA_DBG_WHEN
#define RDDMA_DBG_ALL       (RDDMA_DBG_EVERYONE | RDDMA_DBG_EVERYTHING | RDDMA_DBG_ALWAYS)

#ifdef CONFIG_RDDMA_DEBUG

static void rddma_debug(char *format, ...) __attribute__((unused,format(printf,1,2)));
static void rddma_debug(char *format, ...)
{
	va_list args;
	va_start(args,format);
	vprintk(format,args);
	va_end(args);
}
#define RDDMA_ASSERT(c,f,arg...) if (!(c)) rddma_debug("<%d>" f,0,## arg)
#define RDDMA_DEBUG(l,f, arg...) if ((((l) & RDDMA_DBG_WHEN) <= rddma_debug_level) && (((l) & RDDMA_DBG_WHO_WHAT)) == (l)) rddma_debug("<%d>" f,(l), ## arg)
#define RDDMA_DEBUG_SAFE(l,c,f,arg...) if ((c)) RDDMA_DEBUG((l),f, ## arg)
#else
#define RDDMA_ASSERT(c,f,arg...) do {} while (0)
#define RDDMA_DEBUG(l,f,arg...) do {} while (0)
#define RDDMA_DEBUG_SAFE(l,c,f,arg...) do {} while (0)
#endif

static inline struct kobject *to_kobj(struct list_head *entry)
{
    return entry ? container_of(entry, struct kobject, entry) : NULL;
}

#endif /* RDDMA_H */
