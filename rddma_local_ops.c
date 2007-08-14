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

#define MY_DEBUG      RDDMA_DBG_LOCOPS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_LOCOPS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_location.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_smb.h>
#include <linux/rddma_dst.h>
#include <linux/rddma_src.h>
#include <linux/rddma_xfer.h>
#include <linux/rddma_bind.h>
#include <linux/rddma_binds.h>
#include <linux/rddma_smbs.h>
#include <linux/rddma_xfers.h>
#include <linux/rddma_srcs.h>
#include <linux/rddma_dsts.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_drv.h>

#include <linux/device.h>
#include <linux/mm.h>

/*
 * F I N D    O P E R A T I O N S
 */
static struct rddma_location *rddma_local_location_find(struct rddma_desc_param *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);
	return to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->name));
}

static struct rddma_smb *rddma_local_smb_find(struct rddma_location *parent, struct rddma_desc_param *desc)
{
	return to_rddma_smb(kset_find_obj(&parent->smbs->kset,desc->name));
}

static struct rddma_xfer *rddma_local_xfer_find(struct rddma_location *parent, struct rddma_xfer_param *desc)
{
	return to_rddma_xfer(kset_find_obj(&parent->xfers->kset,desc->xfer.name));
}

static struct rddma_bind *rddma_local_bind_find(struct rddma_xfer *parent, struct rddma_xfer_param *desc)
{
	struct rddma_bind *bind = NULL;
	char *buf = kzalloc(2048,GFP_KERNEL);

	if (NULL == buf)
		goto out;

	if ( 2048 <= snprintf(buf,2048,"%s#%llx:%x",desc->xfer.name, desc->xfer.offset, desc->xfer.extent) )
		goto fail_printf;

	bind = to_rddma_bind(kset_find_obj(&parent->binds->kset,buf));

fail_printf:
	kfree(buf);
out:
	return bind;
}

static struct rddma_dst *rddma_local_dst_find(struct rddma_bind *parent, struct rddma_xfer_param *desc)
{
	return to_rddma_dst(kset_find_obj(&parent->dsts->kset,desc->bind.dst.name));
}

static struct rddma_src *rddma_local_src_find(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
	return to_rddma_src(kset_find_obj(&parent->srcs->kset,desc->bind.src.name));
}

/*
 * C R E A T E     O P E R A T I O N S
 */
static struct rddma_location *rddma_local_location_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);
	return rddma_location_create(loc,desc);
}

static struct rddma_smb *rddma_local_smb_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	return rddma_smb_create(loc,desc);
}

static struct rddma_xfer *rddma_local_xfer_create(struct rddma_location *loc, struct rddma_xfer_param *desc)
{
	struct rddma_dsts *dsts;
	struct rddma_xfer *xfer;
	struct rddma_bind *bind;

	if ( (xfer = rddma_xfer_create(loc,desc)) ) {
		if ( (bind = rddma_bind_create(xfer, desc))) {
			if ( (dsts = bind->desc.dst.ops->dsts_create(bind,desc,"%s#%llx:%x=%s#%llx:%x",
						      desc->bind.dst.name,desc->bind.dst.offset,desc->bind.dst.extent,
						      desc->bind.src.name,desc->bind.src.offset,desc->bind.src.extent)) )
				rddma_xfer_load_binds(xfer,bind);
				return xfer;

			rddma_bind_delete(bind);
		}
		
		rddma_xfer_delete(xfer);
	}

	return xfer;
}

static struct rddma_dsts *rddma_local_dsts_create(struct rddma_bind *parent, struct rddma_xfer_param *desc, char *name, ...)
{
	struct rddma_dsts *dsts = NULL;
	va_list ap;
	
	if (NULL == parent->dsts) {
		va_start(ap,name);
		dsts = rddma_dsts_create_ap(parent,desc,name,ap);
		va_end(ap);

		if (NULL == dsts ) 
			goto out;

		parent->dsts = dsts;
	}

	if ( NULL == rddma_dst_create(parent,desc))
		goto fail_dst;

	rddma_bind_load_dsts(parent);

	return parent->dsts;

fail_dst:
	rddma_dsts_delete(dsts);
out:
	return NULL;
}

static struct rddma_dst *rddma_local_dst_create(struct rddma_bind *parent, struct rddma_xfer_param *desc)
{
	int page, first_page, last_page;
	struct rddma_xfer_param params = *desc;
	struct rddma_smb *ssmb = NULL;
	struct rddma_smb *dsmb = NULL;
	struct rddma_location *sloc = NULL;
	struct rddma_dst *new = NULL;

	if ( NULL == (dsmb = find_rddma_smb(&desc->bind.dst)) )
		goto fail_dsmb;

	if (!DESC_VALID(&dsmb->desc,&desc->bind.dst))
		goto fail_ddesc;

	if ( NULL == (ssmb = find_rddma_smb(&desc->bind.src)) )
		goto fail_ddesc;

	if (!DESC_VALID(&ssmb->desc,&desc->bind.src))
		goto fail_sdesc;

	sloc = find_rddma_location(&desc->bind.src);

	first_page = START_PAGE(&dsmb->desc,&desc->bind.dst);
	last_page = first_page + NUM_DMA(&dsmb->desc,&desc->bind.dst);

/* 	rddma_parse_desc(&params.bind.dst, desc->bind.dst.name); */

	params.bind.dst.offset = START_OFFSET(&dsmb->desc, &desc->bind.dst);
	params.bind.src.extent = params.bind.dst.extent = START_SIZE(&dsmb->desc, &desc->bind.dst);
	
	for (page=first_page; page < last_page; page++) {
		params.bind.dst.offset += (u64)page_address(dsmb->pages[page]);
		new = rddma_dst_create(parent,&params);
		sloc->desc.ops->srcs_create(new,&params);
		params.bind.dst.offset = 0;
		params.bind.dst.extent = PAGE_SIZE;
		params.bind.src.extent += PAGE_SIZE;
	}
	
	rddma_location_put(sloc);
	return new;

fail_sdesc:
	rddma_smb_put(ssmb);
fail_ddesc:
	rddma_smb_put(dsmb);
fail_dsmb:
	return NULL;
}

static struct rddma_srcs *rddma_local_srcs_create(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
/* 	srcs_create://tp.x:2000/d.p#uuuuu000:1000=s.r#c000:1000 */

	int page, first_page, last_page;
	struct rddma_srcs *srcs;
	struct rddma_smb *smb;
	struct rddma_src *src;
	struct rddma_xfer_param params = *desc;

	srcs = rddma_srcs_create(parent,desc);
	smb = find_rddma_smb(&desc->bind.src);

	first_page = START_PAGE(&smb->desc,&desc->bind.src);
	last_page = first_page + NUM_DMA(&smb->desc,&desc->bind.src);

/* 	rddma_parse_desc(&params.bind.src, desc->bind.src.name); */

	params.bind.src.offset = START_OFFSET(&smb->desc, &desc->bind.src);
	params.bind.src.extent = START_SIZE(&smb->desc, &desc->bind.src);
	
	for (page=first_page; page < last_page; page++) {
		params.bind.src.offset += (u64)page_address(smb->pages[page]);
		src = parent->desc.dst.ops->src_create(parent,&params);
		params.bind.src.offset = 0;
		params.bind.src.extent = PAGE_SIZE;
	}
	rddma_dst_load_srcs(parent);
	return srcs;
}

static struct rddma_src *rddma_local_src_create(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
/* 	src_create://tp.x:2000/d.p#uuuuu000:800=s.r#xxxxx800:800 */
	struct rddma_src *src;

	src = rddma_src_create(parent,desc);

	return src;
}

/*
 * D E L E T E    O P E R A T I O N S
 */
static void rddma_local_location_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	rddma_location_delete(loc);
}

static void rddma_local_smb_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	rddma_smb_delete(rddma_local_smb_find(loc,desc));
}

static void rddma_local_xfer_delete(struct rddma_location *loc, struct rddma_xfer_param *desc)
{
	rddma_xfer_delete(rddma_local_xfer_find(loc,desc));
}

static void rddma_local_dst_delete(struct rddma_bind *parent, struct rddma_xfer_param *desc)
{
	rddma_dst_delete(rddma_local_dst_find(parent,desc));
}

static void rddma_local_src_delete(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
	rddma_src_delete(rddma_local_src_find(parent,desc));
}

struct rddma_ops rddma_local_ops = {
	.location_create = rddma_local_location_create,
	.location_delete = rddma_local_location_delete,
	.location_find = rddma_local_location_find,
	.smb_create = rddma_local_smb_create,
	.smb_delete = rddma_local_smb_delete,
	.smb_find = rddma_local_smb_find,
	.xfer_create = rddma_local_xfer_create,
	.xfer_delete = rddma_local_xfer_delete,
	.xfer_find = rddma_local_xfer_find,
	.srcs_create = rddma_local_srcs_create,
	.src_create = rddma_local_src_create,
	.src_delete = rddma_local_src_delete,
	.src_find = rddma_local_src_find,
	.dsts_create = rddma_local_dsts_create,
	.dst_create = rddma_local_dst_create,
	.dst_delete = rddma_local_dst_delete,
	.dst_find = rddma_local_dst_find,
	.bind_find = rddma_local_bind_find,
};

