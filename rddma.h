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

#ifdef CONFIG_RDDMA_DEBUG
extern int rddma_debug_level;
static void rddma_debug(char *format, ...) __attribute__((unused,format(printf,1,2)));
static void rddma_debug(char *format, ...)
{
	va_list args;
	va_start(args,format);
	vprintk(format,args);
	va_end(args);
}
#define RDDMA_ASSERT(c,f,arg...) if (!(c)) rddma_debug("<%d>" f,0,##arg)
#define RDDMA_DEBUG(l,f, arg...) if ((l) < rddma_debug_level) rddma_debug("<%d>" f,(l), ## arg)
#define RDDMA_SAFE_DEBUG(l,c,f,arg...) if (((l) < rddma_debug_level) && (c)) rddma_debug("<%d>" f,(l), ## arg)
#else
#define RDDMA_ASSERT(c,f,arg...) do {} while (0)
#define RDDMA_DEBUG(l,f,arg...) do {} while (0)
#define RDDMA_SAFE_DEBUG(l,c,f,arg...) do {} while (0)
#endif

static inline struct kobject *to_kobj(struct list_head *entry)
{
    return entry ? container_of(entry, struct kobject, entry) : NULL;
}

#endif /* RDDMA_H */
