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

#ifndef RDDMA_PARSE_H
#define RDDMA_PARSE_H
#include <linux/rddma.h>
#include <linux/types.h>
#include <linux/device.h>

/*
 * offset    := <hexstr>
 * extent    := <hexstr>
 * name      := <string>
 * var       := <string>
 * val       := <string>
 * option    := var[=val]
 * options   := ?option[,option]*
 * location  := name[.location][options]
 * smb       := name[.location][options]
 * src | dst := smb[#offset][:extent][options]
 * bind      := dst = src[options]
 * xfer      := name[.location][#offset][:extent] / bind[options]
 */

/**
 * struct rddma_desc_param - The base type structure for RDDMA components
 *@name: A fully qualified name string name[.name]*
 *@location: a pointer to the [.name]* portion of @name
 *@offset: An offset in bytes from the beginning of the component
 *@extent: The size of the component in bytes.
 *@query: An array of the first 5 options as var or var=val strings
 *@rest: A string of the remaining options as var[=val][,var[=val]]*
 *
 *This struct is the fundamental component for naming and describing
 *the properties of share memory buffers and their relationships to
 *each other. An smb has a name, fred, a location,
 *radarinputboard.fabric, and offset and an extent and in its creation
 *you may specify lots of named option flags in an appended query string.
 */
struct rddma_desc_param {
	char *name;
	char *location;
	u64  offset;
	unsigned int extent;
#define RDDMA_MAX_QUERY_STRINGS 5
/* DO NOT SEPERATE THE FOLLOWING TWO LINES */
	char *query[RDDMA_MAX_QUERY_STRINGS];		/* var[=val] */
	char *rest;		/* var[=val][,var[=val]]* */
/* rest is aliased as query[RDDMA_MAX_QUERY_STRINGS] in the parse routine! Naughty code! */
/* private: */
	struct rddma_ops *ops;
	struct rddma_dma_engine *rde;
	struct rddma_fabric_address *address;
};
/**
 * struct rddma_bind_param - A bind is simply dest_desc_param=source_desc_param
 *@dst: the destination rddma_desc_param component of the bind
 *@src: the source rddma_desc_param component of the bind
 *
 * When defining a DMA transfer object a portion of one smb is "bound"
 * to a portion of another smb, one being the source of the dma
 * transfer and the other the destination. These binds are defined by
 * naming the smbs and their locations plus the offsets and extent in
 * each.
 *Example Bind String
 * dst.proc.fabric#4000:1000=src.radar.fabric#8000:1000
 *
 *where #x is the offset and :x is the extent. The extents of the
 *binds on each smb must be equal.
 */
struct rddma_bind_param {
	struct rddma_desc_param dst;
	struct rddma_desc_param src;
};
/**
 * struct rddma_xfer_param - An xfer names the DMA engine and the bind it is to transfer
 *@xfer: An rddma_desc_param describing the name and location of the dma engine to perform the transfer.
 *@bind: An rddma_bind_param describing the bind.
 *
 *A transfer is a string of the form
 *
 * name[.location]* /dst[.location]*=src[.location]*
 *
 * plus any offsets and extents and query option strings required.
 */
struct rddma_xfer_param {
	struct rddma_desc_param xfer;
	struct rddma_bind_param bind;
};

static inline void rddma_inherit(struct rddma_desc_param *c, struct rddma_desc_param *p)
{
	c->address = p->address;
	c->rde = p->rde;
	c->ops = p->ops;
}

extern int __must_check rddma_parse_xfer( struct rddma_xfer_param *, const char *);
extern int __must_check rddma_parse_desc( struct rddma_desc_param *, const char *);
extern char *rddma_get_option( struct rddma_desc_param *, const char *);

extern int rddma_clone_desc(struct rddma_desc_param *new, struct rddma_desc_param *old);
extern int rddma_clone_bind(struct rddma_bind_param *new, struct rddma_bind_param *old);
extern int rddma_clone_xfer(struct rddma_xfer_param *new, struct rddma_xfer_param *old);
extern void rddma_clean_desc(struct rddma_desc_param *);
extern void rddma_clean_xfer(struct rddma_xfer_param *);

#endif	/* RDDMA_PARSE_H */
