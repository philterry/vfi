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
	struct rddma_location *loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->name));
	RDDMA_DEBUG(MY_DEBUG,"%s %p -> %p\n",__FUNCTION__,desc,loc);
	return loc;
}

static struct rddma_smb *rddma_local_smb_find(struct rddma_location *parent, struct rddma_desc_param *desc)
{
	struct rddma_smb *smb = to_rddma_smb(kset_find_obj(&parent->smbs->kset,desc->name));
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,smb);
	return smb;
}

static struct rddma_xfer *rddma_local_xfer_find(struct rddma_location *parent, struct rddma_xfer_param *desc)
{
	struct rddma_xfer *xfer = to_rddma_xfer(kset_find_obj(&parent->xfers->kset,desc->xfer.name));
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,xfer);
	return xfer;
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
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,bind);
	return bind;
}

static struct rddma_dst *rddma_local_dst_find(struct rddma_bind *parent, struct rddma_xfer_param *desc)
{
	struct rddma_dst *dst = to_rddma_dst(kset_find_obj(&parent->dsts->kset,desc->bind.dst.name));
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,dst);
	return dst;
}

static struct rddma_src *rddma_local_src_find(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
	struct rddma_src *src = to_rddma_src(kset_find_obj(&parent->srcs->kset,desc->bind.src.name));
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,src);
	return src;
}

/*
 * C R E A T E     O P E R A T I O N S
 */
static struct rddma_location *rddma_local_location_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	char *dma_engine_name;
	struct rddma_location *newloc = rddma_location_create(loc,desc);

	if ( (dma_engine_name = rddma_get_option(desc,"dma_name")) ) {
		struct rddma_dma_engine *rde = rddma_dma_find(dma_engine_name);
		if ( rde && newloc ) {
			rddma_dma_put(newloc->desc.rde);
			newloc->desc.rde = rde;
		}
		else if (rde)
			rddma_dma_put(rde);
	}

	if (!newloc->desc.rde) {
		rddma_location_put(newloc);
		newloc = NULL;
	}

	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,loc,desc,newloc);

	return newloc;
}

static struct rddma_smb *rddma_local_smb_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_smb *smb = NULL;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	smb = rddma_smb_create(loc, desc);
	if (smb == NULL)
		return NULL;

	/* Allocate memory for this SMB */
	if (rddma_alloc_pages(smb->size, smb->desc.offset, &smb->pages, 
		&smb->num_pages) == 0)
		return smb;

	/* Failed */
	rddma_smb_delete(smb);
	return NULL;
}

static struct rddma_dst *rddma_local_dst_create(struct rddma_bind *parent, struct rddma_xfer_param *desc)
{
	int page, first_page, last_page;
	struct rddma_xfer_param params = *desc;
	struct rddma_location *sloc = NULL;
	struct rddma_smb *dsmb = NULL;
	struct rddma_dst *new = NULL;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if ( NULL == (dsmb = find_rddma_smb(&desc->bind.dst)) )
		goto fail_dsmb;

	if (!DESC_VALID(&dsmb->desc,&desc->bind.dst))
		goto fail_ddesc;

	sloc = find_rddma_location(&desc->bind.src);

	first_page = START_PAGE(&dsmb->desc,&desc->bind.dst);
	last_page = first_page + NUM_DMA(&dsmb->desc,&desc->bind.dst);

	params.bind.dst.offset = START_OFFSET((&dsmb->desc), (&desc->bind.dst));
	params.bind.src.extent = params.bind.dst.extent = START_SIZE(&dsmb->desc, &desc->bind.dst);
	for (page = first_page; page < last_page; page++) {
		params.bind.dst.offset += (unsigned long)page_address(dsmb->pages[page]);
join:
		if (page + 2 <= last_page) {
			if (dsmb->pages[page] + 1 == dsmb->pages[page+1]) {
				if (page + 2 == last_page && END_SIZE(&dsmb->desc,&desc->bind.dst))
					params.bind.dst.extent += END_SIZE(&dsmb->desc, &desc->bind.dst);
				else
					params.bind.dst.extent += PAGE_SIZE;
				params.bind.src.extent = params.bind.dst.extent;
				++page;
				goto join;
			}
		}
		new = rddma_dst_create(parent,&params);
		sloc->desc.ops->srcs_create(new,&params);
		params.bind.dst.offset = 0;
		params.bind.src.offset += params.bind.src.extent;
		if ( page + 2 == last_page && END_SIZE(&dsmb->desc,&desc->bind.dst))
			params.bind.dst.extent = params.bind.src.extent = END_SIZE(&dsmb->desc,&desc->bind.dst);
		else
			params.bind.dst.extent = params.bind.src.extent = PAGE_SIZE;
	} 

	rddma_location_put(sloc);
	return new;

fail_ddesc:
	rddma_smb_put(dsmb);
fail_dsmb:
	return NULL;
}

static struct rddma_dsts *rddma_local_dsts_create(struct rddma_bind *parent, struct rddma_xfer_param *desc, char *name, ...)
{
	struct rddma_dsts *dsts = NULL;
	struct rddma_smb *ssmb = NULL;
	struct rddma_smb *dsmb = NULL;
	va_list ap;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	
	if (NULL == parent->dsts) {
		va_start(ap,name);
		dsts = rddma_dsts_create_ap(parent,desc,name,ap);
		va_end(ap);

		if (NULL == dsts ) 
			goto out;

		parent->dsts = dsts;
	}

	if ( NULL == (dsmb = find_rddma_smb(&desc->bind.dst)) )
		goto fail_dsmb;

	if (!DESC_VALID(&dsmb->desc,&desc->bind.dst))
		goto fail_ddesc;

	if ( NULL == (ssmb = find_rddma_smb(&desc->bind.src)) )
		goto fail_ddesc;

	if (!DESC_VALID(&ssmb->desc,&desc->bind.src))
		goto fail_sdesc;

	rddma_inherit(&parent->desc.src,&ssmb->desc);
	rddma_inherit(&parent->desc.dst,&dsmb->desc);

	if ( NULL == parent->desc.dst.ops->dst_create(parent,desc))
		goto fail_dst;

	rddma_bind_load_dsts(parent);

	return parent->dsts;

fail_dst:
fail_sdesc:
	rddma_smb_put(ssmb);
fail_ddesc:
	rddma_smb_put(dsmb);
fail_dsmb:
	rddma_dsts_delete(dsts);
out:
	return NULL;
}

static struct rddma_xfer *rddma_local_xfer_create(struct rddma_location *loc, struct rddma_xfer_param *desc)
{
	struct rddma_dsts *dsts;
	struct rddma_xfer *xfer;
	struct rddma_bind *bind;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if ( (xfer = rddma_xfer_create(loc,desc)) ) {
		if ( (bind = rddma_bind_create(xfer, desc))) {
			if ( (dsts = rddma_local_dsts_create(bind,desc,"%s#%llx:%x=%s#%llx:%x",
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

static struct rddma_srcs *rddma_local_srcs_create(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
/* 	srcs_create://tp.x:2000/d.p#uuuuu000:1000=s.r#c000:1000 */

	int page, first_page, last_page;
	struct rddma_srcs *srcs;
	struct rddma_smb *smb;
	struct rddma_src *src;
	struct rddma_xfer_param params = *desc;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	parent->srcs = srcs = rddma_srcs_create(parent,desc);
	smb = find_rddma_smb(&desc->bind.src);

	first_page = START_PAGE(&smb->desc,&desc->bind.src);
	last_page = first_page + NUM_DMA(&smb->desc,&desc->bind.src);

	params.bind.src.offset = START_OFFSET(&smb->desc, &desc->bind.src);
	params.bind.src.extent = START_SIZE(&smb->desc, &desc->bind.src);
	for ( page = first_page; page < last_page ; page++ ) {
		params.bind.src.offset += (unsigned long)page_address(smb->pages[page]);
join2:
		if (page + 2 <= last_page) {
			if (smb->pages[page] + 1 == smb->pages[page+1]) {
				if (page + 2 == last_page && END_SIZE(&smb->desc,&desc->bind.src))
					params.bind.src.extent += END_SIZE(&smb->desc, &desc->bind.src);
				else
					params.bind.src.extent += PAGE_SIZE;
				++page;
				goto join2;
			}
		}
		src = parent->desc.dst.ops->src_create(parent,&params);
		params.bind.src.offset = 0;
		if (page + 2 >= last_page && END_SIZE(&smb->desc,&desc->bind.src))
			params.bind.src.extent = END_SIZE(&smb->desc,&desc->bind.src);
		else
			params.bind.src.extent = PAGE_SIZE;
	} 

	rddma_dst_load_srcs(parent);
	return srcs;
}

static struct rddma_src *rddma_local_src_create(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
/* 	src_create://tp.x:2000/d.p#uuuuu000:800=s.r#xxxxx800:800 */
	struct rddma_src *src;
	RDDMA_DEBUG(MY_DEBUG,"%s %s#%llx:%x\n",__FUNCTION__,desc->bind.src.name, desc->bind.src.offset,desc->bind.src.extent);

	src = rddma_src_create(parent,desc);

	return src;
}


/*
 * D E L E T E    O P E R A T I O N S
 */
static void rddma_local_location_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	rddma_location_delete(loc);
}

static void rddma_local_smb_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_smb *smb = rddma_local_smb_find(loc,desc);
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	if (smb) {
		rddma_smb_put(smb);
		rddma_smb_delete(smb);
	}
}

static int rddma_local_xfer_delete(struct rddma_location *loc, struct rddma_xfer_param *desc)
{
	struct rddma_xfer *xfer;
	int ret = -EINVAL;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	xfer = rddma_local_xfer_find(loc,desc);
	if (xfer) {
		kobject_put(&xfer->kobj);
		rddma_xfer_delete(xfer);
		ret = 0;
	}
	return ret;
}

static void rddma_local_dst_delete(struct rddma_bind *parent, struct rddma_xfer_param *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	rddma_dst_delete(rddma_local_dst_find(parent,desc));
}

static void rddma_local_src_delete(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
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

