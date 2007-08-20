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

#define MY_DEBUG      RDDMA_DBG_FABOPS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_FABOPS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

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

static struct rddma_location *rddma_fabric_location_find(struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	RDDMA_DEBUG(MY_DEBUG,"%s entered with %s\n",__FUNCTION__,desc->orig_desc);

	if ( (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->name))) )
		return loc;

	if (desc->location)
		if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->location))) )
			return NULL;
	
	if (!loc)
		loc = new_rddma_location(NULL,desc);

	skb = rddma_fabric_call(loc, 5, "location_find://%s", desc->name);
	rddma_location_put(loc);
	
	loc = NULL;

	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				 loc = find_rddma_location(&reply);
			kfree(reply.name);
		}
	}

	return loc;
}

static struct rddma_smb *rddma_fabric_smb_find(struct rddma_location *parent, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_smb *smb = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "smb_find://%s", desc->name);

	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				smb = rddma_smb_create(loc,&reply);
			kfree(reply.name);
		}
	}

	return smb;
}

static struct rddma_xfer *rddma_fabric_xfer_find(struct rddma_location *parent, struct rddma_xfer_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_xfer *xfer = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "xfer_find://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				xfer =  rddma_xfer_create(loc,&reply);
			kfree(reply.xfer.name);
		}
	}

	return xfer;
}

static struct rddma_bind *rddma_fabric_bind_find(struct rddma_xfer *parent, struct rddma_xfer_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_bind *bind = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "bind_find://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				bind = rddma_bind_create(parent,&reply);
			kfree(reply.xfer.name);
		}
	}

	return bind;
}

static struct rddma_dst *rddma_fabric_dst_find(struct rddma_bind *parent, struct rddma_xfer_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_dst *dst = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "dst_find://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				dst = rddma_dst_create(parent,&reply);
			kfree(reply.xfer.name);
		}
	}

	return dst;
}

static struct rddma_src *rddma_fabric_src_find(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_src *src = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "src_find://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				src = rddma_src_create(parent,&reply);
			kfree(reply.xfer.name);
		}
	}

	return src;
}

/*
 * C R E A T E     O P E R A T I O N S
 */
static struct rddma_location *rddma_fabric_location_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb = NULL;
	struct rddma_location *newloc = NULL;
	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

	if (loc)
		newloc = loc;
	else {
		char *fabric_name;
		if ( (fabric_name =rddma_get_option(desc,"fabric")) ) {
			struct rddma_fabric_address *rfa = rddma_fabric_find(fabric_name);
			if ( rfa ) {
				newloc = new_rddma_location(NULL,desc);
				newloc->desc.address = rfa;
			}
		}   
	}

	if (newloc)
		skb = rddma_fabric_call(newloc, 5, "location_create://%s", desc->name);

	if (!loc)
		rddma_location_put(newloc);

	newloc = NULL;

	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				newloc = rddma_location_create(loc,&reply);
			kfree(reply.name);
		}
	}

	return newloc;
}

static struct rddma_smb *rddma_fabric_smb_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_smb *smb = NULL;

	skb = rddma_fabric_call(loc, 5, "smb_create://%s", desc->name);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				smb = rddma_smb_create(loc,&reply);
			kfree(reply.name);
		}
	}

	return smb;
}

static struct rddma_xfer *rddma_fabric_xfer_create(struct rddma_location *loc, struct rddma_xfer_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_xfer *xfer = NULL;

	skb = rddma_fabric_call(loc, 5, "xfer_create://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				xfer = rddma_xfer_create(loc,&reply);
			kfree(reply.xfer.name);
		}
	}

	return xfer;
}

static struct rddma_dsts *rddma_fabric_dsts_create(struct rddma_bind *parent, struct rddma_xfer_param *desc, char *name, ...)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_dsts *dsts = NULL;

	va_list ap;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "dsts_create://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0) {
				va_start(ap,name);
				dsts = rddma_dsts_create_ap(parent,&reply,name,ap);
				va_end(ap);
			}
			kfree(reply.xfer.name);
		}
	}

	return dsts;
}

static struct rddma_dst *rddma_fabric_dst_create(struct rddma_bind *parent, struct rddma_xfer_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_dst *dst = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "dst_create://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				dst = rddma_dst_create(parent,&reply);
			kfree(reply.xfer.name);
		}
	}

	return dst;
}

static struct rddma_srcs *rddma_fabric_srcs_create(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_srcs *srcs = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "srcs_create://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				srcs = rddma_srcs_create(parent,&reply);
			kfree(reply.xfer.name);
		}
	}

	return srcs;
}

static struct rddma_src *rddma_fabric_src_create(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_src *src = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "src_create://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				src = rddma_src_create(parent,&reply);
			kfree(reply.xfer.name);
		}
	}

	return src;
}

/*
 * D E L E T E    O P E R A T I O N S
 */
static void rddma_fabric_location_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;

	skb = rddma_fabric_call(loc, 5, "location_delete://%s", desc->name);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				rddma_location_delete(loc);
			kfree(reply.name);
		}
	}
}

static void rddma_fabric_smb_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_smb *smb;

	if (NULL == (smb = to_rddma_smb(kset_find_obj(&loc->smbs->kset,desc->name))) )
		return;

	skb = rddma_fabric_call(loc, 5, "smb_delete://%s", desc->name);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				rddma_smb_delete(smb);
			kfree(reply.name);
		}
	}
}

static void rddma_fabric_xfer_delete(struct rddma_location *loc, struct rddma_xfer_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_xfer *xfer;

	if (NULL == (xfer = to_rddma_xfer(kset_find_obj(&loc->xfers->kset,desc->xfer.name))) )
		return;

	skb = rddma_fabric_call(loc, 5, "xfer_delete://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				rddma_xfer_delete(xfer);
			kfree(reply.xfer.name);
		}
	}
}

static void rddma_fabric_dst_delete(struct rddma_bind *parent, struct rddma_xfer_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_dst *dst;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return;

	if (NULL == (dst = to_rddma_dst(kset_find_obj(&parent->dsts->kset,desc->xfer.name))) )
		return;

	skb = rddma_fabric_call(loc, 5, "dst_delete://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				rddma_dst_delete(dst);
			kfree(reply.xfer.name);
		}
	}
}

static void rddma_fabric_src_delete(struct rddma_dst *parent, struct rddma_xfer_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_src *src;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return;

	if (NULL == (src = to_rddma_src(kset_find_obj(&parent->srcs->kset,desc->xfer.name))) )
		return;

	skb = rddma_fabric_call(loc, 5, "src_delete://%s/%s=%s", desc->xfer.name,desc->bind.dst.name,desc->bind.src.name);
	if (skb) {
		struct rddma_xfer_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_xfer(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				rddma_src_delete(src);
			kfree(reply.xfer.name);
		}
	}
}

struct rddma_ops rddma_fabric_ops = {
	.location_create = rddma_fabric_location_create,
	.location_delete = rddma_fabric_location_delete,
	.location_find = rddma_fabric_location_find,
	.smb_create = rddma_fabric_smb_create,
	.smb_delete = rddma_fabric_smb_delete,
	.smb_find = rddma_fabric_smb_find,
	.xfer_create = rddma_fabric_xfer_create,
	.xfer_delete = rddma_fabric_xfer_delete,
	.xfer_find = rddma_fabric_xfer_find,
	.srcs_create = rddma_fabric_srcs_create,
	.src_create = rddma_fabric_src_create,
	.src_delete = rddma_fabric_src_delete,
	.src_find = rddma_fabric_src_find,
	.dsts_create = rddma_fabric_dsts_create,
	.dst_create = rddma_fabric_dst_create,
	.dst_delete = rddma_fabric_dst_delete,
	.dst_find = rddma_fabric_dst_find,
	.bind_find = rddma_fabric_bind_find,
};

