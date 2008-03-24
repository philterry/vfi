/* 
 * 
 * Copyright 2008 Vmetro
 * Phil Terry <pterry@vmetro.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef VFI_H
#define VFI_H

#include <linux/kobject.h>
#include <stdarg.h>

#define CONFIG_VFI_DEBUG

/* Merge DMA descriptors for adjacent pages.
 * This should always be on, unless you're trying
 * to generate long DMA chains for testing.
 */
#define OPTIMIZE_DESCRIPTORS

extern unsigned int vfi_debug_level;
/* Debug level is treated as a four-bit level integer, when, and a 28-bit mask, who_what */
#define VFI_DBG_WHEN 0xf
#define VFI_DBG_WHO_WHAT ~VFI_DBG_WHEN

/* The when level follows syslog conventions: */
#define	VFI_DBG_EMERG	        0	/* system is unusable			*/
#define	VFI_DBG_ALERT	        1	/* action must be taken immediately	*/
#define	VFI_DBG_CRIT	        2	/* critical conditions			*/
#define	VFI_DBG_ERR	        3	/* error conditions			*/
#define	VFI_DBG_WARNING	4	/* warning conditions			*/
#define	VFI_DBG_NOTICE	5	/* normal but significant condition	*/
#define	VFI_DBG_INFO	        6	/* informational			*/
#define	VFI_DBG_DEBUG	        7	/* debug-level messages			*/

/* The top-bits of who_what select who, a susbsystem, component, type etc. */
#define VFI_DBG_LOCATION 0x80000000
#define VFI_DBG_SMB      0x40000000
#define VFI_DBG_XFER     0x20000000
#define VFI_DBG_CDEV     0x10000000
#define VFI_DBG_FABRIC   0x08000000
#define VFI_DBG_FABNET   0x04000000
#define VFI_DBG_OPS      0x02000000
#define VFI_DBG_LOCOPS   0x01000000
#define VFI_DBG_FABOPS   0x00800000
#define VFI_DBG_PARSE    0x00400000
#define VFI_DBG_DMA      0x00200000
#define VFI_DBG_DMARIO   0x00100000
#define VFI_DBG_SUBSYS   0x00080000
#define VFI_DBG_BIND     0x00040000
#define VFI_DBG_DST      0x00020000
#define VFI_DBG_SRC      0x00010000
#define VFI_DBG_MMAP     0x00008000
#define VFI_DBG_RDYS     0x00004000
#define VFI_DBG_DONES    0x00002000
#define VFI_DBG_EVNTS    0x00001000
#define VFI_DBG_EVNT     0x00000800
#define VFI_DBG_DMANET   0x00000400
#define VFI_DBG_SYNC     0x00000200
#define VFI_DBG_WHO      0xfffffe00

/* The bottom bits of who_what select a functional area, what to debug */
#define VFI_DBG_FUNCALL  0x00000010
#define VFI_DBG_LIFE     0x00000020
#define VFI_DBG_DMA_CHAIN   0x00000040
#define VFI_DBG_ERROR    0x00000080
#define VFI_DBG_WHAT     (~VFI_DBG_WHO & VFI_DBG_WHO_WHAT)

/* So from msb to lsb debug_level can be thought of as defining who, what, when */
/* eg Location operators, function calls, when info is wanted */
/* VFI_DBG_LOCATION | VFI_DBG_FUNCALL | VFI_DBG_INFO */
#define VFI_DBG_EVERYONE   VFI_DBG_WHO
#define VFI_DBG_EVERYTHING VFI_DBG_WHAT
#define VFI_DBG_ALWAYS     VFI_DBG_WHEN
#define VFI_DBG_ALL       (VFI_DBG_EVERYONE | VFI_DBG_EVERYTHING | VFI_DBG_ALWAYS)

#define VFI_DBG_SYSLOG_LEVEL "<1>"
#ifdef CONFIG_VFI_DEBUG

static void vfi_debug(char *format, ...) __attribute__((unused,format(printf,1,2)));
static void vfi_debug(char *format, ...)
{
	va_list args;
	va_start(args,format);
	vprintk(format,args);
	va_end(args);
}
#define VFI_ASSERT(c,f,arg...) if (!(c)) vfi_debug(VFI_DBG_SYSLOG_LEVEL f,## arg)
#define VFI_DEBUG(l,f, arg...) if ((((l) & VFI_DBG_WHEN) <= (vfi_debug_level & VFI_DBG_WHEN)) && \
				     ((((l) & VFI_DBG_WHO_WHAT) & vfi_debug_level ) == ((l) & VFI_DBG_WHO_WHAT)) ) vfi_debug(VFI_DBG_SYSLOG_LEVEL f, ## arg)
#define VFI_DEBUG_SAFE(l,c,f,arg...) if ((c)) VFI_DEBUG((l),f, ## arg)
static inline int vfi_error(int level, int err)
{
	if (err)
		VFI_DEBUG(level,"%s:%d:%s returns error %d\n",__FILE__,__LINE__, __FUNCTION__,err);
	return err;
}
#define VFI_RESULT(x) vfi_error(MY_ERROR,(x))
#else
#define VFI_ASSERT(c,f,arg...) do {} while (0)
#define VFI_DEBUG(l,f,arg...) do {} while (0)
#define VFI_DEBUG_SAFE(l,c,f,arg...) do {} while (0)
#define VFI_RESULT(x) (x)
#endif

#ifdef CONFIG_VFI_KOBJ_DEBUG
#define VFI_KTRACE(arg...) printk (arg)
#else
#define VFI_KTRACE(arg...) do {} while (0)
#endif


/* Helper for printing sysfs attributes... define your show function (...char *buffer...) with {int left = PAGE_SIZE;int size = 0; .... return size;} */
/* then ... can include ATTR_PRINTF("format string",...); */
#define ATTR_PRINTF(f,arg...) size += snprintf(buffer+size,left,f, ## arg); left -=size
#define MACADDRFMT "%x:%x:%x:%x:%x:%x"
#define MACADDRBYTES(mac) (unsigned char)mac[0],(unsigned char)mac[1],(unsigned char)mac[2],(unsigned char)mac[3],(unsigned char)mac[4],(unsigned char)mac[5]

static inline struct kobject *to_kobj(struct list_head *entry)
{
    return entry ? container_of(entry, struct kobject, entry) : NULL;
}

#endif /* VFI_H */
