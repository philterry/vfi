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

#ifndef VFI_PARSE_H
#define VFI_PARSE_H
#include <linux/vfi.h>
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
 * struct vfi_desc_param - The base type structure for VFI components
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
struct vfi_desc_param {
	char *buf;
	size_t buflen;
	char *name;
	char *location;
	u64  offset;
	unsigned int extent;
	char *soffset;		/* Pointer to offset string */
	char *sextent;		/* Pointer to extent string */
	char **query;
	char **end;
/* private: */
	struct vfi_ops *ops;
	struct vfi_dma_engine *rde;
	struct vfi_fabric_address *address;
	struct vfi_location *ploc;
};

/**
 * struct vfi_xfer_param - An xfer names the DMA engine and the bind it is to transfer
 *@xfer: An vfi_desc_param describing the name and location of the dma engine to perform the transfer.
 *@bind: An vfi_bind_param describing the bind.
 *
 *A transfer is a string of the form
 *
 * name[.location]* /dst[.location]*=src[.location]*
 *
 * plus any offsets and extents and query option strings required.
 */
struct vfi_bind_param {
	struct vfi_desc_param xfer;
	struct vfi_desc_param dst;
	struct vfi_desc_param src;
};

static inline void vfi_inherit(struct vfi_desc_param *c, struct vfi_desc_param *p)
{
	c->address = p->address;
	c->rde = p->rde;
	c->ops = p->ops;
	c->ploc = p->ploc;
}
static inline void vfi_bind_inherit(struct vfi_bind_param *c, struct vfi_bind_param *p)
{
	vfi_inherit(&c->xfer,&p->xfer);
	vfi_inherit(&c->dst,&p->dst);
	vfi_inherit(&c->src,&p->src);
}

extern int __must_check vfi_parse_bind( struct vfi_bind_param *, const char *);
extern int __must_check vfi_parse_desc( struct vfi_desc_param *, const char *);
extern char *vfi_get_option( struct vfi_desc_param *, const char *);

extern int vfi_clone_desc(struct vfi_desc_param *new, struct vfi_desc_param *old);
extern int vfi_clone_bind(struct vfi_bind_param *new, struct vfi_bind_param *old);
extern void vfi_clean_desc(struct vfi_desc_param *);
extern void vfi_clean_bind(struct vfi_bind_param *);

#endif	/* VFI_PARSE_H */
