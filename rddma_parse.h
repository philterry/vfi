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
 * option    := var[(<val>)]
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
	char *buf;
	size_t buflen;
	char *name;
	char *location;
	u64  offset;
	unsigned int extent;
	char **query;
	char **end;
/* private: */
	struct rddma_ops *ops;
	struct rddma_dma_engine *rde;
	struct rddma_fabric_address *address;
	struct rddma_location *ploc;
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
struct rddma_bind_param {
	struct rddma_desc_param xfer;
	struct rddma_desc_param dst;
	struct rddma_desc_param src;
};

static inline void rddma_inherit(struct rddma_desc_param *c, struct rddma_desc_param *p)
{
	c->address = p->address;
	c->rde = p->rde;
	c->ops = p->ops;
	c->ploc = p->ploc;
}
static inline void rddma_bind_inherit(struct rddma_bind_param *c, struct rddma_bind_param *p)
{
	rddma_inherit(&c->xfer,&p->xfer);
	rddma_inherit(&c->dst,&p->dst);
	rddma_inherit(&c->src,&p->src);
}

extern int __must_check rddma_parse_bind( struct rddma_bind_param *, const char *);
extern int __must_check rddma_parse_desc( struct rddma_desc_param *, const char *);
extern char *rddma_get_option( struct rddma_desc_param *, const char *);

extern int rddma_clone_desc(struct rddma_desc_param *new, struct rddma_desc_param *old);
extern int rddma_clone_bind(struct rddma_bind_param *new, struct rddma_bind_param *old);
extern void rddma_clean_desc(struct rddma_desc_param *);
extern void rddma_clean_bind(struct rddma_bind_param *);

#endif	/* RDDMA_PARSE_H */
