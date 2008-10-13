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

#define MY_DEBUG      VFI_DBG_FABOPS | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_FABOPS | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_FABOPS | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_location.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_dst.h>
#include <linux/vfi_src.h>
#include <linux/vfi_xfer.h>
#include <linux/vfi_bind.h>
#include <linux/vfi_smbs.h>
#include <linux/vfi_xfers.h>
#include <linux/vfi_srcs.h>
#include <linux/vfi_dsts.h>
#include <linux/vfi_syncs.h>
#include <linux/vfi_sync.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_drv.h>
#include <linux/vfi_event.h>
#include <linux/device.h>
#include <linux/mm.h>

/*
 * F I N D    O P E R A T I O N S
 */
extern void vfi_dma_chain_dump(struct list_head *h);
extern void bind_param_dump(struct vfi_bind_param *);

static int vfi_fabric_location_find(struct vfi_location **newloc, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *oldloc = NULL ;
	struct vfi_location *myloc = NULL;
	int ret;

	*newloc = NULL;

	VFI_DEBUG(MY_DEBUG,"%s %p %p %s,%s\n",__FUNCTION__,loc,desc,desc->name,desc->location);

	ret = find_vfi_name(&oldloc,loc,desc);
	if (!ret) {
		*newloc = oldloc;
		return VFI_RESULT(ret);
	}

	if (loc) {
		ret = vfi_fabric_call(&skb, loc, 5, "location_find://%s.%s", desc->name,desc->location);
	}
	else {
	        struct vfi_location *uniqueloc = kzalloc(sizeof(struct vfi_location), GFP_KERNEL);
        	if (NULL == uniqueloc)
                	return VFI_RESULT(-ENOMEM);

		vfi_clone_desc(&uniqueloc->desc,desc);
		uniqueloc->desc.ops = &vfi_local_ops;
	        kobject_set_name(&uniqueloc->kset.kobj,"%p", &uniqueloc);
		uniqueloc->kset.kobj.ktype = &vfi_location_type;
		uniqueloc->kset.kobj.kset = &vfi_subsys->kset;
		ret = kset_register(&uniqueloc->kset);
		if (!ret) { 
			ret = vfi_fabric_call(&skb, uniqueloc, 5, "location_find://%s", desc->name);
			if (ret != -ENOMEM)
				vfi_address_unregister(uniqueloc);
		}
		vfi_location_put(uniqueloc);
	}
	if (ret)
		return VFI_RESULT(ret);

	VFI_DEBUG(MY_DEBUG,"%s skb(%p)\n",__FUNCTION__,skb);

	if (skb) {
		struct vfi_desc_param reply;

		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0 && reply.extent) {
				if (loc) {
					if (loc->desc.extent) {
						desc->offset = reply.offset;
						desc->extent = loc->desc.extent;
					}
					else {
						desc->offset = reply.offset;
						loc->desc.extent = desc->extent = reply.offset;
						desc->ops = &vfi_local_ops;
					}
				}
				else {
					desc->offset = reply.offset;
					desc->extent = 0;
				}

				vfi_clean_desc(&reply);

				if (oldloc) {
					oldloc->desc.extent = desc->extent;
					oldloc->desc.offset = desc->offset;
					*newloc = oldloc;
					return VFI_RESULT(0);
				}

				ret = vfi_location_create(&myloc,loc,desc);
				if (ret) { 
					if (ret != -EEXIST)
						return VFI_RESULT(ret);

					ret = find_vfi_name(&myloc,loc,desc);
					if (ret)
						return VFI_RESULT(ret);

					if (myloc->desc.ops && myloc->desc.ops->location_lose)
						myloc->desc.ops->location_lose(myloc,desc);

					ret = myloc->smbs ? 0 : new_vfi_smbs(&myloc->smbs,"smbs",myloc);
					if (ret == 0 || ret == -EEXIST) {
						ret = myloc->xfers ? 0 : new_vfi_xfers(&myloc->xfers,"xfers",myloc);
						if (ret == 0 || ret == -EEXIST) {
							ret = myloc->syncs ? 0 : new_vfi_syncs(&myloc->syncs,"syncs",myloc);
						}
					}

					if (ret && ret != -EEXIST) {
						vfi_location_put(myloc);
						return VFI_RESULT(ret);
					}
				}
				else if (loc && loc->desc.address)
					loc->desc.address->ops->register_location(myloc);

				*newloc = myloc;
				return VFI_RESULT(0);
			}
			vfi_clean_desc(&reply);
		}
	}

	return VFI_RESULT(-EINVAL);
}

static void vfi_fabric_location_put(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	vfi_location_put(loc);
}

static void vfi_fabric_location_lose(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct sk_buff *skb = (struct sk_buff *) NULL;
	struct vfi_location *uniqueloc;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if (!loc) {
		VFI_DEBUG(MY_ERROR,"\t%s called with null location\n",__FUNCTION__);
		return;
	}

	uniqueloc = kzalloc(sizeof(struct vfi_location), GFP_KERNEL);
        if (NULL == uniqueloc) {
		VFI_DEBUG(MY_ERROR,"\t%s failed to allocate unique temporary local location\n",__FUNCTION__);
               	return;
	}

	vfi_clone_desc(&uniqueloc->desc,desc);
	uniqueloc->desc.ops = &vfi_local_ops;
	kobject_set_name(&uniqueloc->kset.kobj,"%p", &uniqueloc);
	uniqueloc->kset.kobj.ktype = &vfi_location_type;
	uniqueloc->kset.kobj.kset = &vfi_subsys->kset;
	ret = kset_register(&uniqueloc->kset);
	if (!ret) {
		ret = vfi_fabric_call(&skb, uniqueloc, 5, "location_put://%s.%s", desc->name,desc->location);
		if (ret != -ENOMEM)
			vfi_address_unregister(uniqueloc);
		if (ret)
			VFI_DEBUG(MY_ERROR,"\t%s vfi_fabric_call ret(%d)\n",__FUNCTION__,ret);
	}
	else VFI_DEBUG(MY_ERROR,"\t%s failed to register unique temporary local location ret(%d)\n",__FUNCTION__,ret);
	vfi_location_put(uniqueloc);
	
	VFI_DEBUG(MY_DEBUG,"%s skb(%p)\n",__FUNCTION__,skb);

	if (skb) {
		struct vfi_desc_param reply;

		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) != 1) || ret) {
				VFI_DEBUG(MY_ERROR,"\t%s location_put result(%d)\n",__FUNCTION__,ret);
			}
			vfi_clean_desc(&reply);
		}
	}
}

static int vfi_fabric_smb_find(struct vfi_smb **smb, struct vfi_location *parent, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if ( (*smb = to_vfi_smb(kset_find_obj(&parent->smbs->kset,desc->name))) )
		return VFI_RESULT(0);

	ret = vfi_fabric_call(&skb, parent, 5, "smb_find://%s.%s", desc->name,desc->location);

	if (skb) {
		struct vfi_desc_param reply;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0) {
				ret = vfi_smb_create(smb,parent,&reply);
				if (ret == -EEXIST) {
					if ( (*smb = to_vfi_smb(kset_find_obj(&parent->smbs->kset,desc->name))) ) {
						if ((*smb)->desc.ops && (*smb)->desc.ops->smb_put) 
							(*smb)->desc.ops->smb_put(*smb,desc);
						ret = 0;
					}
				}
			}
			vfi_clean_desc(&reply);
		}
	}

	return VFI_RESULT(ret);
}

static void vfi_fabric_smb_put(struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_fabric_call(&skb, smb->desc.ploc, 5, "smb_put://%s.%s", smb->desc.name,smb->desc.location);

	if (skb) {
		struct vfi_desc_param reply;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				vfi_smb_delete(smb);
			vfi_clean_desc(&reply);
		}
	}
}

static void vfi_fabric_smb_lose(struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	/*
	 * This function gets called when the reference count of the smb component reaches 0, and 
	 * the kobject's release function is invoked. It sends a private method over the fabric.
	 */
	struct sk_buff  *skb;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_fabric_call(&skb, smb->desc.ploc, 5, "smb_lose://%s.%s", smb->desc.name,smb->desc.location);

	if (skb) {
		struct vfi_desc_param reply;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				VFI_DEBUG(MY_DEBUG,"%s:%d\n",__FUNCTION__,__LINE__);
			vfi_clean_desc(&reply);
		}
	}
}

static int vfi_fabric_mmap_find(struct vfi_mmap **mmap, struct vfi_smb *parent, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	return VFI_RESULT(-EINVAL);
}

/**
* vfi_fabric_xfer_find - find, or create, an vfi_xfer object for a named xfer at a remote location
* @loc	: location where xfer officially resides
* @desc	: target xfer parameter descriptor
*
* This function finds an vfi_xfer object for the xfer described by @desc, which officially resides
* at a remote fabric location defined by @loc.
*
* If the xfer is found to exist at that site then a stub will be created for the xfer in the local tree.
*
* The function returns a pointer to the vfi_xfer object that represents the target xfer in the
* local tree. It will return NULL if no such xfer exists at the remote site.
*
**/
static int vfi_fabric_xfer_find(struct vfi_xfer **xfer, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb = NULL;
	int ret;

	VFI_DEBUG (MY_DEBUG, "%s (%s)\n", __func__, ((desc) ? ((desc->name) ? : "<UNK>") : "<NULL>"));

	if ( (*xfer = to_vfi_xfer(kset_find_obj(&loc->xfers->kset,desc->name))) )
		return VFI_RESULT(0);

	ret = vfi_fabric_call(&skb, loc, 5, "xfer_find://%s.%s",
				desc->name,desc->location
				);
	if (skb) {
		struct vfi_desc_param reply;
		ret = -EINVAL;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0) {
				reply.extent = 0;
				reply.offset = 0;
				ret =  vfi_xfer_create(xfer,loc,&reply);
			}
			vfi_clean_desc(&reply);
		}
	}

	return VFI_RESULT(ret);
}

static void vfi_fabric_xfer_put(struct vfi_xfer *xfer, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_fabric_call(&skb, xfer->desc.ploc, 5, "xfer_put://%s.%s", xfer->desc.name,xfer->desc.location);

	if (skb) {
		struct vfi_desc_param reply;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				vfi_xfer_put(xfer);
			vfi_clean_desc(&reply);
		}
	}
}

static void vfi_fabric_xfer_lose(struct vfi_xfer *xfer, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	/*
	 * Send xfer_lose request to remote <xfer> agent using the 
	 * offset and extent values specified in the descriptor. 
	 * The request will end up in the local xfer_lose. This will decrement 
	 * the reference count of the object and if it reaches 0, the
	 * object local release function will be called.
	 */
	ret = vfi_fabric_call(&skb, xfer->desc.ploc, 5, "xfer_lose://%s.%s", xfer->desc.name,xfer->desc.location);

	if (skb) {
		struct vfi_desc_param reply;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				VFI_DEBUG(MY_DEBUG,"%s:%d\n",__FUNCTION__,__LINE__);
			vfi_clean_desc(&reply);
		}
	}
}

/**
* vfi_fabric_sync_find - find, or create, an vfi_sync object for a named sync at a remote location
* @loc	: location where sync officially resides
* @desc	: target sync parameter descriptor
*
* This function finds an vfi_sync object for the sync described by @desc, which officially resides
* at a remote fabric location defined by @loc.
*
* If the sync is found to exist at that site then a stub will be created for the sync in the local tree.
*
* The function returns a pointer to the vfi_sync object that represents the target sync in the
* local tree. It will return NULL if no such sync exists at the remote site.
*
**/
static int vfi_fabric_sync_find(struct vfi_sync **sync, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	VFI_DEBUG (MY_DEBUG, "%s (%s)\n", __func__, ((desc) ? ((desc->name) ? : "<UNK>") : "<NULL>"));

	/*
	* Look for an existing stub for the target sync in the local tree.
	* 
	*/
	if ( (*sync = to_vfi_sync(kset_find_obj(&loc->syncs->kset,desc->name))) )
		return VFI_RESULT(0);

	/*
	* If no such sync object currently exists at that site, then deliver an 
	* "sync_find" request to the [remote] destination to look for it there.
	*/
	ret = vfi_fabric_call(&skb, loc, 5, "sync_find://%s.%s",
				desc->name,desc->location
				);
	/*
	* We need a reply...
	*/
	if (skb) {
		struct vfi_desc_param reply;
		ret = -EINVAL;
		/*
		* Parse the reply into a descriptor...
		*/
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			/*
			* ...and if the reply indicates success, create a local sync object
			* and bind it to the location. Thus our sysfs tree now has local knowledge
			* of this sync's existence, though it live somewhere else.
			*/
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0) {
				reply.extent = 0;
				reply.offset = 0;
				ret = vfi_sync_create(sync,loc,&reply);
				if (ret == -EEXIST) {
					if ( (*sync = to_vfi_sync(kset_find_obj(&loc->syncs->kset,desc->name))) ) {
						if ((*sync)->desc.ops && (*sync)->desc.ops->sync_put) 
							(*sync)->desc.ops->sync_put(*sync,desc);
						ret = 0;
					}
				}
			}
			vfi_clean_desc(&reply);
		}
	}

	return VFI_RESULT(ret);
}

/**
* vfi_fabric_sync_put - find, or create, an vfi_sync object for a named sync at a remote location
* @loc	: location where sync officially resides
* @desc	: target sync parameter descriptor
*
* This function finds an vfi_sync object for the sync described by @desc, which officially resides
* at a remote fabric location defined by @loc.
*
* If the sync is found to exist at that site then a stub will be created for the sync in the local tree.
*
* The function returns a pointer to the vfi_sync object that represents the target sync in the
* local tree. It will return NULL if no such sync exists at the remote site.
*
**/
static void vfi_fabric_sync_put(struct vfi_sync *sync, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	VFI_DEBUG (MY_DEBUG, "%s %p %p\n", __func__, sync, desc);

	ret = vfi_fabric_call(&skb, sync->desc.ploc, 5, "sync_put://%s.%s",
				desc->name,desc->location
				);
	if (skb) {
		struct vfi_desc_param reply;
		ret = -EINVAL;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0) {
				ret = 0;
				vfi_sync_put(sync);
			}
			vfi_clean_desc(&reply);
		}
	}
}

static int vfi_fabric_sync_send(struct vfi_sync *sync, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	ret = vfi_fabric_call(&skb, sync->desc.ploc, 5, "sync_send://%s.%s#%llx",
			      sync->desc.name,sync->desc.location, desc->offset
				);
	/*
	* We need a reply...
	*/
	if (skb) {
		struct vfi_desc_param reply;
		ret = -EINVAL;
		/*
		* Parse the reply into a descriptor...
		*/
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			sscanf(vfi_get_option(&reply,"result"),"%d",&ret);
			vfi_clean_desc(&reply);
		}
	}

	return VFI_RESULT(ret);
}

static int vfi_fabric_sync_wait(struct vfi_sync *sync, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	ret = vfi_fabric_call(&skb, sync->desc.ploc, 5, "sync_wait://%s.%s#%llx",
			      sync->desc.name,sync->desc.location,desc->offset
				);
	/*
	* We need a reply...
	*/
	if (skb) {
		struct vfi_desc_param reply;
		ret = -EINVAL;
		/*
		* Parse the reply into a descriptor...
		*/
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			sscanf(vfi_get_option(&reply,"result"),"%d",&ret);
			vfi_clean_desc(&reply);
		}
	}

	return VFI_RESULT(ret);
}

/**
* vfi_fabric_bind_find - find a bind belonging to a specified [remote] xfer
* @parent : pointer to <xfer> object that bind belongs to
* @desc   : <xfer> specification string
*
* Let's reiterate a point made in comments at the local version of this function: the bind
* we are seeking is not implicitly named. Rather we construct a name using the offset and extent
* values of the xfer it belongs to... values which, given the dynamic nature of xfer offset
* and extent, the originator of the bind search should have frigged to match the dimensions of the
* bind to be found. You pays your money, you takes your chance.
*
* What we do know for sure is that if the bind object exists at all, it does so at the <xfer> site.
* and that is where we deliver this bind_find instruction.
*
* The function uses the reply it receives from <xfer> to create a stub for the bind in the local tree.
* It returns a pointer to that object to the caller.
*
**/
static int vfi_fabric_bind_find(struct vfi_bind **bind, struct vfi_xfer *parent, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	char buf[128];
	int ret;
	*bind = NULL;

	snprintf(buf,128,"#%llx:%x",desc->offset,desc->extent);

	VFI_DEBUG(MY_DEBUG,"%s parent(%p) desc(%p) bind(%s)\n",__FUNCTION__,parent,desc,buf);

	if ( (*bind = to_vfi_bind(kset_find_obj(&parent->kset,buf))) )
		return VFI_RESULT(0);

	ret = vfi_fabric_call(&skb, parent->desc.ploc, 5, "bind_find://%s.%s#%llx:%x",
				desc->name,desc->location,desc->offset,desc->extent);
	/*
	* What we get back in reply should be a complete specification of the bind, 
	* including its <xfer>, <dst>, and <src> components. This allows us to instantiate
	* a local stub for the bind (just the bind) within our object tree.
	*/
	if (skb) {
		struct vfi_bind_param reply;
		ret = -EINVAL;
		if (!vfi_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0) {
				ret = vfi_bind_create(bind,parent,&reply);
			}
			vfi_clean_bind(&reply);
		}
	}

	return VFI_RESULT(ret);
}

extern int vfi_local_dst_find(struct vfi_dst **, struct vfi_bind *, struct vfi_bind_param *);
static int vfi_fabric_dst_find(struct vfi_dst **dst, struct vfi_bind *parent, struct vfi_bind_param *desc)
{
	struct sk_buff  *skb;
	int ret;
	struct vfi_location *loc = parent->desc.dst.ploc;
	ret = vfi_local_dst_find(dst,parent,desc);

	VFI_DEBUG(MY_DEBUG,"%s parent(%p) desc(%p) dst(%p)\n",__FUNCTION__,parent,desc,dst);

	if (*dst)
		return VFI_RESULT(0);

	ret = vfi_fabric_call(&skb,loc, 5, "dst_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	if (skb) {
		struct vfi_bind_param reply;
		ret = -EINVAL;
		if (!vfi_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0) {
				ret = vfi_dst_create(dst,parent,&reply);
				/*
				 * Don't forget that this is a find function so the caller assumes a "get"
				 * is performed on the searched object. If the object has just been created
				 * do an artificial get
				*/
				vfi_dst_get(*dst);
			}
			vfi_clean_bind(&reply);
		}
	}

	return VFI_RESULT(ret);
}

static int vfi_fabric_src_find(struct vfi_src **src, struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *loc = parent->desc.src.ploc;
	int ret;

	*src = NULL;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_fabric_call(&skb,loc, 5, "src_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	if (skb) {
		struct vfi_bind_param reply;
		ret = -EINVAL;
		if (!vfi_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0) {
				ret = vfi_src_create(src,parent,&reply);
				/*
				 * Don't forget that this is a find function so the caller assumes a "get"
				 * is performed on the searched object. If the object has just been created
				 * do an artificial get
				*/
				vfi_src_get(*src);
			}
			vfi_clean_bind(&reply);
		}
	}

	return VFI_RESULT(ret);
}

/*
 * C R E A T E     O P E R A T I O N S
 */

/**
* vfi_fabric_location_create - Create a fabric location
* @desc   : <location create> specification string
*
* Create a public location i.e a location with default_ops specified
* as public.
* If we are not to use the top level location as a name
* service we will declare it public. The top level location 
* must be created with offset and extent #0:0 (address) so we 
* will not register an address, but just add the node to sysfs 
* so we can find it later. Creating a lower level location as
* public is only meaningful if the top level location is private
* (name service enabled).
**/
static int vfi_fabric_location_create(struct vfi_location **newloc,struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret;
	struct vfi_location *oldloc;
	*newloc = NULL;

	VFI_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

	/* Check if the location already exists */
	ret = find_vfi_name(&oldloc,loc,desc);
	if (!ret) {
		vfi_location_put(oldloc);
		return VFI_RESULT(-EEXIST);
	}

	if (!loc) {
		/* Create a top level location as public */
		if (desc->extent || desc->offset)
			return VFI_RESULT(-EINVAL);

		return VFI_RESULT(vfi_location_create(newloc,NULL,desc));
		//return VFI_RESULT(new_vfi_location(newloc,NULL,desc));
	}

	/* Create a second level or more location */
	/* TODO: Check that root location is private */
	ret = vfi_location_create(newloc,loc,desc);
	if (*newloc && (*newloc)->desc.address)
		(*newloc)->desc.address->ops->register_location(*newloc);

	return VFI_RESULT(ret);
}

/**
* vfi_fabric_smb_create - create SMB at remote location
* @loc
* @desc
*
* This function implements "smb_create" when the specified location
* for the buffer is some other node on the VFI network, reachable
* through the interconnect fabric.
*
**/
static int vfi_fabric_smb_create(struct vfi_smb **smb, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_fabric_call(&skb, loc, 5, "smb_create://%s.%s#%llx:%x", desc->name,desc->location,desc->offset, desc->extent);
	if (skb) {
		struct vfi_desc_param reply;
		ret = -EINVAL;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				ret = vfi_smb_create(smb,loc,&reply);
			vfi_clean_desc(&reply);
		}
	}

	return VFI_RESULT(ret);
}

static int vfi_fabric_mmap_create(struct vfi_mmap **mmap,struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	return VFI_RESULT(-EINVAL);
}

/**
* vfi_fabric_bind_create
* @parent : pointer to <xfer> object that bind belongs to
* @desc   : <xfer> specification string
*
* bind_create must always run on a bind <xfer> agent.
*
* This function delivers bind_create to the <xfer> agent specified new bind. On return it will
* create a local stub for the bind in the object tree slung beneath the local stub of its parent
* <xfer>.
*
* The <xfer> object MUST exist in the local tree before this function is called.
*
**/
static int vfi_fabric_bind_create(struct vfi_bind **bind, struct vfi_xfer *parent, struct vfi_bind_param *desc)
{
	struct sk_buff  *skb;
	int ret;
	struct vfi_location *loc = parent->desc.ploc;	/* Parent xfer location */
	*bind = NULL;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_fabric_call(&skb,loc, 5, "bind_create://%s.%s#%llx:%x/%s.%s#%llx:%x?event_name(%s)=%s.%s#%llx:%x?event_name(%s)",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,vfi_get_option(&desc->dst,"event_name"),
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent,vfi_get_option(&desc->src,"event_name"));
	/*
	* Provided we receive a reply, one that can be parsed into a bind descriptor 
	* AND that indicates success at the remote site, then we may create a local
	* stub for the bind and insert it into our local tree.
	*/
	if (skb) {
		struct vfi_bind_param reply;
		ret = -EINVAL;
		if (!vfi_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				ret = vfi_bind_create(bind,parent,&reply);
			vfi_clean_bind(&reply);
		}
	}

	return VFI_RESULT(ret);
}

static int vfi_fabric_xfer_create(struct vfi_xfer **xfer, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	*xfer = NULL;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_fabric_call(&skb,loc, 5, "xfer_create://%s.%s#%llx:%x",
				desc->name,desc->location,desc->offset,desc->extent
				);
	if (skb) {
		struct vfi_desc_param reply;
		ret = -EINVAL;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				ret = vfi_xfer_create(xfer,loc,&reply);
			vfi_clean_desc(&reply);
		}
	}

	return VFI_RESULT(ret);
}

static int vfi_fabric_sync_create(struct vfi_sync **sync, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	*sync = NULL;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_fabric_call(&skb,loc, 5, "sync_create://%s.%s#%llx:%x",
				desc->name,desc->location,desc->offset,desc->extent
				);
	if (skb) {
		struct vfi_desc_param reply;
		ret = -EINVAL;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				ret = vfi_sync_create(sync,loc,&reply);
			vfi_clean_desc(&reply);
		}
	}

	return VFI_RESULT(ret);
}

/**
* vfi_fabric_dst_events - create <dst> events when <xfer> is remote
* @bind - bind object, subject of operation
* @desc - descriptor of the bind
*
* This function creates two <dst> events - dst_ready, and dst_done - when the
* bind <xfer> and <dst> are located at different network locations.
*
* It is always the <xfer> dst_events() op that is invoked, but this function always
* runs at the <dst> site, and is only ever invoked by vfi_local_dsts_create().
*
* Since <xfer> and <dst> occupy different locations, events are raised by delivering
* doorbells:
*
* - <xfer> will issue the dst_ready doorbell whose identity is embedded in @desc as "event_id(x)"
* - <dst> will raise the dst_done doorbell whose identity is allocated here, and reported to <xfer>
*   in fabric command replies.
*
**/
static int vfi_fabric_dst_events(struct vfi_bind *bind, struct vfi_bind_param *desc)
{
	/* Local destination SMB with a remote transfer agent. */
	char *event_str;
	char *event_name;
	int event_id = -1;
	struct vfi_events *event_list;
	int ret;
	
	VFI_DEBUG(MY_DEBUG,"%s bind(%p) desc(%p)\n",__FUNCTION__,bind,desc);

	bind->dst_ready_event = NULL;
	
	/*
	* The common event name is embedded in the command string as an
	* "event_name(n)" qualifier. The dst_ready doorbell identifier, allocated
	* by <xfer>, is embedded in the command as an "event_id(x)" qualifier.
	*
	*/
	event_str = vfi_get_option(&desc->dst,"event_id");
	event_name = vfi_get_option(&desc->dst,"event_name");

	if (event_str && event_name) {
		sscanf(event_str,"%d",&event_id);

		ret = find_vfi_events(&event_list,vfi_subsys->events, event_name);
		if (event_list == NULL)
			ret = vfi_events_create(&event_list,vfi_subsys->events,event_name);

		ret = vfi_event_create(&bind->dst_ready_event,event_list,&desc->xfer,bind,bind->desc.xfer.ops->dst_ready,event_id);
		bind->dst_ready_event_id = event_id;
		
		/*
		* The dst_done event identifier must be obtained by registering a new
		* doorbell. The event id is returned by the registration function.
		*
		*/
		event_id = vfi_doorbell_register(bind->desc.xfer.address,
						   (void (*)(void *))bind->desc.dst.ops->dst_done,
						   (void *)bind);

		ret = vfi_event_create(&bind->dst_done_event,event_list,&desc->dst,bind,0,event_id);
		bind->dst_done_event_id = event_id;
		return VFI_RESULT(0);
	}
	return VFI_RESULT(-EINVAL);
}

/**
* vfi_fabric_dst_ev_delete - delete <dst> events when <xfer> is remote
* @bind : parent bind object
* @desc : bind descriptor
*
* This function complements vfi_fabric_dst_events() by providing the means to
* delete two <dst> events when <xfer> lives elsewhere on the network.
*
* It is always the <xfer> dst_ev_delete() op that is invoked, but this function always
* runs at the <dst> site, and is only ever invoked by vfi_local_dsts_delete().
*
**/
static void vfi_fabric_dst_ev_delete (struct vfi_bind *bind, struct vfi_bind_param *desc)
{
	VFI_DEBUG (MY_DEBUG, "%s: for %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", __func__,
	             desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
	             desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent,
	             desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);
	VFI_DEBUG (MY_DEBUG, "dst_ready (%p, %08x), dst_done (%p, %08x)\n",
	             bind->dst_ready_event, bind->dst_ready_event_id,
	             bind->dst_done_event, bind->dst_done_event_id);
	vfi_event_delete (bind->dst_ready_event);
	vfi_event_delete (bind->dst_done_event);
	vfi_doorbell_unregister (bind->desc.xfer.address, bind->dst_done_event_id);
	bind->dst_ready_event = bind->dst_done_event = NULL;
	bind->dst_ready_event_id = bind->dst_done_event_id = 0;
}

/**
* vfi_fabric_dsts_create
* @parent : parent bind object
* @desc	  : bind descriptor
*
* dsts_create always runs on the bind <dst> location.
*
**/
static int vfi_fabric_dsts_create(struct vfi_dsts **dsts, struct vfi_bind *parent, struct vfi_bind_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *loc = parent->desc.dst.ploc;
	int event_id = -1;
	struct vfi_bind_param reply;
	int ret = -EINVAL;
	char *event_str;
	char *dst_event_name;
	char *src_event_name;

	/*
	* Both the dst and the src must be bound to an event, identified
	* by name as part of the bind descriptor.
	*
	*/
	dst_event_name = vfi_get_option(&parent->desc.dst,"event_name");
	src_event_name = vfi_get_option(&parent->desc.src,"event_name");

	if (dst_event_name == NULL || src_event_name == NULL)
		goto event_fail;

	event_id = vfi_doorbell_register(parent->desc.xfer.address,
					   (void (*)(void *))parent->desc.xfer.ops->dst_ready,
					   (void *)parent);
	if (event_id < 0)
		goto event_fail;

	/*
	* Create the dsts object, then send dsts_create to
	* the remote <dst> site.
	*
	* We do NOT build-out the dsts subtree further; at least
	* not directly. The only way we will ever see a subtree is
	* if the <src> location turns out to be here.
	* 
	* CAUTION:
	* --------
	* If this function is called from <xfer> during local_bind_create,
	* as should always happen, then vfi_dsts_create will ALREADY have
	* been called and succeeded as a pre-condition to invoking this 
	* dst->dsts_create fabric call.
	*
	* It happens that vfi_dsts_create will NOT attempt to create a duplicate
	* <dsts>, but it will re-register the existing <dsts>, doubling-up on 
	* associated counts.
	*/
	ret = vfi_dsts_create(dsts,parent,desc);

	if (ret || *dsts == NULL)
		goto dsts_fail;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_fabric_call(&skb, loc, 5, "dsts_create://%s.%s#%llx:%x/%s.%s#%llx:%x?event_id(%d),event_name(%s)=%s.%s#%llx:%x?event_name(%s)",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				event_id,dst_event_name,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent,
				src_event_name);
	/*
	* On return...
	*
	* We do need to parse the reply, provided it indicates a successful
	* creation. The reply will specify the bind in terms of its <xfer>, <dst>, 
	* and <src> components, but should include the identity of any event
	* associated with the <dst> component. This is the identity of the event
	* the <dst> location will raise when the <dst> part of a bind has completed.
	*/
	if (skb == NULL)
		goto skb_fail;
	
	if (vfi_parse_bind(&reply,skb->data)) 
			goto parse_fail;

	if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret != 0)
		goto result_fail;

	event_str = vfi_get_option(&reply.dst,"event_id");

	if (event_str == NULL)
		goto result_fail;

	if (sscanf(event_str,"%d",&parent->dst_done_event_id) != 1)
		goto result_fail;

	parent->dst_ready_event_id = event_id;

	vfi_bind_load_dsts(parent);

#ifdef CONFIG_VFI_DEBUG
	if (vfi_debug_level & VFI_DBG_DMA_CHAIN)
		vfi_dma_chain_dump(&parent->dma_chain);
#endif

	parent->end_of_chain = parent->dma_chain.prev;

	dev_kfree_skb(skb);
	vfi_clean_bind(&reply);
	return VFI_RESULT(0);

result_fail:
	vfi_clean_bind(&reply);
parse_fail:
	dev_kfree_skb(skb);
skb_fail:
	vfi_dsts_delete(*dsts);
dsts_fail:
	vfi_doorbell_unregister(parent->desc.xfer.address, event_id);
event_fail:
	return VFI_RESULT(-EINVAL);
}

/**
* vfi_fabric_dst_create
* @parent : bind object that dst belongs to
* @desc   : bind descriptor, identifying <xfer>, <dst>, and <src> elements of the bind.
*
* This function is invoked to deliver a dst_create command to a bind <xfer> agent.
*
* dst_create always runs on the bind <xfer> agent.
*
**/
static int vfi_fabric_dst_create(struct vfi_dst **dst, struct vfi_bind *parent, struct vfi_bind_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *loc = parent->desc.xfer.ploc;	/* dst_create runs on <xfer> */
	char *src_event_name;
	int ret = -EINVAL;
	*dst = NULL;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	src_event_name = vfi_get_option(&desc->src,"event_name");

	if (src_event_name == NULL)
		goto fail_event;

	ret = vfi_fabric_call(&skb,loc, 5, "dst_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_name(%s)",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent,
				src_event_name);
	if (skb) {
		struct vfi_bind_param reply;
		ret = -EINVAL;
		if (!vfi_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0) {
				ret = vfi_local_dst_find(dst,parent,&reply);
				if (NULL == *dst)
					ret = vfi_dst_create(dst,parent,&reply);
			}
			vfi_clean_bind(&reply);
		}
	}

fail_event:
	return VFI_RESULT(ret);
}

/**
* vfi_fabric_src_events
* @parent : pointer to <dst> object that bind belongs to
* @desc   : <bind> specification string
*
* src_events must always run on a bind <xfer> agent.
*
**/
static int vfi_fabric_src_events(struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	/* Local source SMB with a remote transfer agent. */
	struct vfi_bind *bind;
	char *event_str;
	char *event_name;
	int event_id = -1;
	struct vfi_events *event_list;
	int ret;

	bind = vfi_dst_parent(parent);

	if (bind->src_ready_event)
		return VFI_RESULT(0);
	
	event_str = vfi_get_option(&desc->src,"event_id");
	event_name = vfi_get_option(&desc->src,"event_name");

	if (event_str && event_name) {
		sscanf(event_str,"%d",&event_id);

		ret = find_vfi_events(&event_list,vfi_subsys->events, event_name);
		if (event_list == NULL)
			ret = vfi_events_create(&event_list,vfi_subsys->events,event_name);

		ret = vfi_event_create(&bind->src_ready_event,event_list,&desc->xfer,bind,bind->desc.xfer.ops->src_ready,event_id);
		bind->src_ready_event_id = event_id;
		
		event_id = vfi_doorbell_register(bind->desc.xfer.address,
						   (void (*)(void *))bind->desc.src.ops->src_done,
						   (void *)bind);

		ret = vfi_event_create(&bind->src_done_event, event_list,&desc->src,bind,0,event_id);
		bind->src_done_event_id = event_id;
		return VFI_RESULT(0);
	}
	return VFI_RESULT(-EINVAL);
}

/**
*
*
*
**/
static void vfi_fabric_src_ev_delete (struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	struct vfi_bind *bind = vfi_dst_parent (parent);
	
	VFI_DEBUG (MY_DEBUG, "%s: for %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", __func__,
	             desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
	             desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent,
	             desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);
	VFI_DEBUG (MY_DEBUG, "src_ready (%p, %08x), src_done (%p, %08x)\n",
	             bind->src_ready_event, bind->src_ready_event_id,
	             bind->src_done_event, bind->src_done_event_id);
	vfi_event_delete (bind->src_ready_event);
	vfi_event_delete (bind->src_done_event);
	vfi_doorbell_unregister (bind->desc.xfer.address, bind->src_done_event_id);
	bind->src_ready_event = bind->src_done_event = NULL;
	bind->src_ready_event_id = bind->src_done_event_id = 0;
}




/**
* vfi_fabric_srcs_create
* @parent : pointer to <bind> object that srcs belongs to
* @desc   : <bind> specification string
*
*
* srcs_create must always run on a bind <src> agent, with a fabric call
* placed by the <xfer> agent.
*
**/
static int vfi_fabric_srcs_create(struct vfi_srcs **srcs, struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *sloc = parent->desc.src.ploc;
	int event_id = -1 ;
	struct vfi_bind *bind;
	struct vfi_bind_param reply;
	int ret = -EINVAL;
	char *event_str;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	event_str = vfi_get_option(&desc->src,"event_name");
	
	if (event_str == NULL)
		goto out;

	bind = vfi_dst_parent(parent);

	if (bind == NULL)
		goto out;

	if (bind->src_ready_event == NULL) {
		bind->src_ready_event_id = event_id = vfi_doorbell_register(bind->desc.xfer.address,
									   (void (*)(void *))bind->desc.xfer.ops->src_ready,
									   (void *)bind);
		if (event_id < 0)
			goto event_fail;
	}

	ret = vfi_srcs_create(srcs,parent,desc);

	if (*srcs == NULL)
		goto srcs_fail;

	ret = vfi_fabric_call(&skb,sloc, 5, "srcs_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_name(%s),event_id(%d)",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent,event_str,bind->src_ready_event_id);
	if (ret)
		goto skb_fail;

	ret = -EINVAL;

	if (skb == NULL)
		goto skb_fail;

	if (vfi_parse_bind(&reply,skb->data)) 
		goto parse_fail;

	if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret != 0)
		goto result_fail;

	if (bind->src_done_event_id < 0) {
		event_str = vfi_get_option(&reply.src,"event_id");

		if (event_str == NULL)
			goto result_fail;

		if (sscanf(event_str,"%d",&bind->src_done_event_id) != 1)
			goto result_fail;
	}

	vfi_dst_load_srcs(parent);

	vfi_clean_bind(&reply);
	dev_kfree_skb(skb);
	return VFI_RESULT(ret);

result_fail:
	vfi_clean_bind(&reply);
parse_fail:
	dev_kfree_skb(skb);
skb_fail:
	vfi_srcs_delete(*srcs);
srcs_fail:
	if (event_id != -1)
		vfi_doorbell_unregister(bind->desc.xfer.address,event_id);
event_fail:
	vfi_bind_put(bind);
out:
	return VFI_RESULT(ret);
}

/**
* vfi_fabric_src_create
* @parent : pointer to <dst> object that src belongs to
* @desc   : <bind> specification string
*
* src_create must always run on a bind <xfer> agent, and should be invoked
* by the <src> agent while constructing <srcs> for a specific <dst>.
*
**/
static int vfi_fabric_src_create(struct vfi_src **src, struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *sloc = parent->desc.xfer.ploc;	/* src_create runs on <xfer> */
	int ret;

	*src = NULL;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_fabric_call(&skb,sloc, 5, "src_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	ret = -EINVAL;
	
	if (skb) {
		struct vfi_bind_param reply;
		if (!vfi_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				ret = vfi_src_create(src,parent,&reply);
			vfi_clean_bind(&reply);
		}
	}

	return VFI_RESULT(ret);
}

/*
 * D E L E T E    O P E R A T I O N S
 */
static void vfi_fabric_location_delete(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	vfi_fabric_call(&skb,loc, 5, "location_delete://%s.%s", desc->name,desc->location);

	if (skb) {
		struct vfi_desc_param reply;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				vfi_location_delete(loc);
			vfi_clean_desc(&reply);
		}
	}
}

static void vfi_fabric_smb_delete(struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s (%s.%s)\n",__FUNCTION__, desc->name, desc->location);

	/*
	 * Just put the reference count of the local cache of the smb component 
	 * if the kobject is released the local release function will be called
	 * which will send smb_lose to the remote.
	 */
	vfi_smb_delete(smb);
}

static void vfi_fabric_mmap_delete(struct vfi_mmap *mmap, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

}

static void vfi_fabric_mmap_put(struct vfi_mmap *mmap, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

}

/**
* vfi_fabric_bind_delete - send bind_delete request to remote Xfer agent
* @xfer : pointer to xfer to be deleted, with ploc identifying <xfer> agent location
* @desc : descriptor of the bind to be deleted
*
* bind_delete must always run on the bind <xfer> agent.
*
* This function is invoked to deliver a bind_delete request to a remote bind <xfer> agent.
* Upon successful conclusion of the remote op, it will then delete the local instance of
* the subject bind.
*
**/
static void vfi_fabric_bind_delete(struct vfi_xfer *xfer, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *loc = xfer->desc.ploc;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	/*
	 * Send bind_delete request to remote <xfer> agent using the 
	 * offset and extent values specified in the descriptor. 
	*/
	vfi_fabric_call(&skb,loc, 5, "bind_delete://%s.%s#%llx:%x",
			desc->name, desc->location, desc->offset, desc->extent);
	if (skb) {
		struct vfi_desc_param reply;
		int ret = -EINVAL;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0) {
				vfi_bind_delete(xfer, desc);
				VFI_DEBUG(MY_DEBUG,"%s:%d\n",__FUNCTION__,__LINE__);
			}
			vfi_clean_desc(&reply);
		}
	}
}

/**
* vfi_fabric_bind_lose - send bind_lose request to remote Xfer agent
* @xfer : pointer to xfer to be deleted, with ploc identifying <xfer> agent location
* @desc : descriptor of the bind to be put
*
* bind_lose must always run on the bind <xfer> agent.
*
* This function decrement the reference count of the object and if it reaches 0, the
* object release function will be called which in turn will send a delete over the fabric.
* Delete is now a private method and should not be used by the user
*
**/
static void vfi_fabric_bind_lose(struct vfi_xfer *xfer, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *loc = xfer->desc.ploc;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	/*
	 * Send bind_lose request to remote <xfer> agent using the 
	 * offset and extent values specified in the descriptor. 
	 * The request will end up in the local bind_lose. This will decrement 
	 * the reference count of the object and if it reaches 0, the
	 * object local release function will be called.
	 */
	vfi_fabric_call(&skb,loc, 5, "bind_lose://%s.%s#%llx:%x",
				desc->name, desc->location, desc->offset, desc->extent);
	if (skb) {
		struct vfi_desc_param reply;
		int ret = -EINVAL;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				VFI_DEBUG(MY_DEBUG,"%s:%d\n",__FUNCTION__,__LINE__);
			vfi_clean_desc(&reply);
		}
	}
}

static void vfi_fabric_xfer_delete(struct vfi_xfer *xfer, struct vfi_desc_param *desc)
{

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	/*
	 * Just put the reference count of the local cache of the xfer component 
	 * if the kobject is released the local release function will be called
	 * which will send xfer_lose to the remote.
	 */
	vfi_xfer_delete(xfer,NULL);

}

static void vfi_fabric_sync_delete(struct vfi_sync *sync, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	vfi_fabric_call(&skb,sync->desc.ploc, 5, "sync_delete://%s.%s", desc->name,desc->location);
	if (skb) {
		struct vfi_desc_param reply;
		int ret = -EINVAL;
		if (!vfi_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				vfi_sync_delete(sync,&reply);
			vfi_clean_desc(&reply);

		}
	}
}

/**
* vfi_fabric_dst_delete
* @parent : pointer to <bind> object that dst belongs to
* @desc   : <bind> specification string
*
* dst_delete must always run on the bind <xfer> agent, and ought to be invoked while
* executing dsts_delete on the <dst> agent.
*
**/
static void vfi_fabric_dst_delete(struct vfi_bind *bind, struct vfi_bind_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *loc = bind->desc.xfer.ploc;	/* dst_delete runs on <xfer> */
	
	/*
	* HACK ALERT:
	* -----------
	* When deleting <dst> over fabric, we want to place a hold on deleting the local
	* instance until we return. We want to do this just in case both <dst> and <src>
	* are co-located somewhere outside the <xfer> agent - in which case an srcs_delete
	* will be sent back here meantime that we do NOT want to destroy the hierarchy that
	* we are currently working on.
	*
	* A simple find is sufficient to bump-up the refcount. We shall undo it in a moment.
	*
	*/
	struct vfi_dst *dst = NULL;
	int ret;
	
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	VFI_KTRACE ("<*** %s find dst before fabric call IN ***>\n", __func__);
	find_vfi_dst_in (&dst,bind, desc);
	VFI_KTRACE ("<*** %s find dst before fabric call OUT ***>\n", __func__);
	
	vfi_fabric_call(&skb,loc, 5, "dst_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x", 
	                        desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
	                        desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent,
	                        desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);
	
	if (dst) vfi_dst_put (dst);
	if (skb) {
		struct vfi_bind_param reply;
		if (!vfi_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				vfi_dst_delete(bind,&reply);
			vfi_clean_bind(&reply);
		}
	}
}

/**
* vfi_fabric_src_delete
* @parent : pointer to <dst> object that src belongs to
* @desc   : <bind> specification string
*
* src_delete must always run on the bind <xfer> agent
**/
static void vfi_fabric_src_delete(struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *loc = parent->desc.xfer.ploc;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	
	/*
	* Compose and deliver src_delete command.
	*
	* Note the unusual composition of the bind parameters to be embedded in the command; how the <dst>
	* section is built from the parent spec, while <xfer> and <src> are built from the descriptor explicitly
	* passed to us. That is a bugfix to ensure that <xfer> is sent the same source bind specification on
	* src_delete that it was with src_create - when <src> is remote, the <dst> component in the explicit 
	* bind descriptor contains a base <dst> offset, not <dst> SMB address. 
	*
	* I have no idea how the discrepancy arose - probably because the srcs_delete code does not perform
	* xfer/bind/dst find calls the way that srcs_create does? 
	*
	*/
	vfi_fabric_call(&skb,loc, 5, "src_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x", 
	                        desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
	                        parent->desc.dst.name, parent->desc.dst.location, parent->desc.dst.offset, parent->desc.dst.extent,
	                        desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);
	
	if (skb) {
		struct vfi_bind_param reply;
		if (!vfi_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				vfi_src_delete(parent,&reply);
			vfi_clean_bind(&reply);
		}
	}
	
}

/**
* vfi_fabric_srcs_delete
* @parent : pointer to <dst> object that srcs belong to
* @desc   : <bind> specification string
*
* srcs_delete must always run on the bind <src> agent.
*
**/
static void vfi_fabric_srcs_delete(struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *loc = parent->desc.src.ploc;	/* srcs_delete runs on <src> */
	struct vfi_bind *bind = vfi_dst_parent (parent);
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	vfi_fabric_call(&skb,loc, 5, "srcs_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
				desc->dst.name,  desc->dst.location,  desc->dst.offset,  desc->dst.extent,
				desc->src.name,  desc->src.location,  desc->src.offset,  desc->src.extent);
	if (skb) {
		struct vfi_bind_param reply;
		if (!vfi_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0) {
				vfi_doorbell_unregister (bind->desc.xfer.address, bind->src_ready_event_id);
				vfi_srcs_delete(parent->srcs);
			}
			vfi_clean_bind(&reply);
		}
	}
}

/**
* vfi_fabric_dsts_delete - send dsts_delete request to bind <dst> agent
* @parent : pointer to bind object that is being deleted
* @desc   : descriptor of the bind being deleted
*
* dsts_delete must always run on a bind <dst> agent.
*
* This function, which runs as part of bind_delete, will deliver a dsts_delete request
* to the <dst> agent of a bind being deleted. It should ONLY be invoked at that time, as
* an integral part of bind_delete.
*
* The request is composed and delivered to the <dst> agent. Upon successful return, the local
* instance of <dsts> is deleted too.
*
**/
static void vfi_fabric_dsts_delete (struct vfi_bind *parent, struct vfi_bind_param *desc)
{
	struct sk_buff  *skb;
	struct vfi_location *loc = parent->desc.dst.ploc;	/* dsts_delete runs on <dst> */

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	vfi_fabric_call(&skb,loc, 5, "dsts_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
				desc->dst.name,  desc->dst.location,  desc->dst.offset,  desc->dst.extent,
				desc->src.name,  desc->src.location,  desc->src.offset,  desc->src.extent);
	if (skb) {
		struct vfi_bind_param reply;
		if (!vfi_parse_bind(&reply,skb->data)) {
			int ret = -EINVAL;
			dev_kfree_skb(skb);
			if ( (sscanf(vfi_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0) {
				vfi_doorbell_unregister (parent->desc.xfer.address, parent->dst_ready_event_id);
			}
			vfi_dsts_delete(parent->dsts);
			vfi_clean_bind(&reply);
		}
	}
}

static void vfi_fabric_done(struct vfi_event *event)
{
	/* Nothing to be done here mate */
}

static void vfi_fabric_src_done(struct vfi_bind *bind)
{
	struct vfi_fabric_address *address;
	
	/* Our local DMA engine, has completed a transfer involving a
	 * remote SMB as the source requiring us to send a done event
	 * to the remote source so that it may adjust its votes. */
	VFI_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	vfi_bind_src_done(bind);
	address = (bind->desc.src.address) ? : ((bind->desc.src.ploc) ? bind->desc.src.ploc->desc.address : NULL);
	vfi_doorbell_send(address, bind->src_done_event_id);
}

static void vfi_fabric_dst_done(struct vfi_bind *bind)
{
	struct vfi_fabric_address *address;
	
	/* Our local DMA engine, has completed a transfer involving a
	 * remote SMB as the destination requiring us to send a done
	 * event to the remote destination so that it may adjust its
	 * votes. */
	VFI_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	vfi_bind_dst_done(bind);
	address = (bind->desc.dst.address) ? : ((bind->desc.dst.ploc) ? bind->desc.dst.ploc->desc.address : NULL);
	vfi_doorbell_send(address, bind->dst_done_event_id);
}

static void vfi_fabric_src_ready(struct vfi_bind *bind)
{
	struct vfi_fabric_address *address;

	/* Someone locally executed start on an event associated with
	 * the local source SMB in a bind assigned a remote DMA
	 * engine. So we need to send the ready event to it so that it
	 * may adjust its vote accordingly. */
	VFI_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	vfi_bind_src_ready(bind);
	address = (bind->desc.xfer.address) ? : ((bind->desc.xfer.ploc) ? bind->desc.xfer.ploc->desc.address : NULL);
	vfi_doorbell_send(address,bind->src_ready_event_id);
}

static void vfi_fabric_dst_ready(struct vfi_bind *bind)
{
	struct vfi_fabric_address *address;

	/* Someone locally executed start on an event associated with
	 * the local destination SMB in a bind assigned a remote DMA
	 * engine. So we need to send the ready event to it so that it
	 * may adjust its vote accordingly. */
	VFI_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	vfi_bind_dst_ready(bind);
	address = (bind->desc.xfer.address) ? : ((bind->desc.xfer.ploc) ? bind->desc.xfer.ploc->desc.address : NULL);
	vfi_doorbell_send(address,bind->dst_ready_event_id);
}

static int vfi_fabric_event_start(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret = -EINVAL;

	VFI_DEBUG(MY_DEBUG,"%s loc(%p) desc(%p)\n",__FUNCTION__, loc, desc);

	vfi_fabric_call(&skb,loc, 5, "event_start://%s.%s", desc->name,desc->location);
	if (skb) {
		struct vfi_desc_param reply;
		if (!vfi_parse_desc(&reply,skb->data)) {
			sscanf(vfi_get_option(&reply,"result"),"%d",&ret);
			vfi_clean_desc(&reply);
		}
		dev_kfree_skb(skb);
	}
	return VFI_RESULT(ret);
}

static int vfi_fabric_event_find(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret = -EINVAL;

	VFI_DEBUG(MY_DEBUG,"%s loc(%p) desc(%p)\n",__FUNCTION__, loc, desc);

	vfi_fabric_call(&skb,loc, 5, "event_find://%s.%s", desc->name,desc->location);
	if (skb) {
		struct vfi_desc_param reply;
		if (!vfi_parse_desc(&reply,skb->data)) {
			sscanf(vfi_get_option(&reply,"result"),"%d",&ret);
			vfi_clean_desc(&reply);
		}
		dev_kfree_skb(skb);
	}
	return VFI_RESULT(ret);
}

static int vfi_fabric_event_chain(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret = -EINVAL;

	VFI_DEBUG(MY_DEBUG,"%s loc(%p) desc(%p)\n",__FUNCTION__, loc, desc);

	vfi_fabric_call(&skb,loc, 5, "event_chain://%s.%s?event_name(%s)", desc->name,desc->location,vfi_get_option(desc,"event_name"));
	if (skb) {
		struct vfi_desc_param reply;
		if (!vfi_parse_desc(&reply,skb->data)) {
			sscanf(vfi_get_option(&reply,"result"),"%d",&ret);
			vfi_clean_desc(&reply);
		}
		dev_kfree_skb(skb);
	}
	return VFI_RESULT(ret);
}

struct vfi_ops vfi_fabric_ops = {
	.location_create = vfi_fabric_location_create,
	.location_delete = vfi_fabric_location_delete,
	.location_find   = vfi_fabric_location_find,
	.location_put    = vfi_fabric_location_put,
	.location_lose   = vfi_fabric_location_lose,
	.smb_create      = vfi_fabric_smb_create,
	.smb_delete      = vfi_fabric_smb_delete,
	.smb_find        = vfi_fabric_smb_find,
	.smb_put         = vfi_fabric_smb_put,
	.smb_lose        = vfi_fabric_smb_lose,
	.mmap_create     = vfi_fabric_mmap_create,
	.mmap_delete     = vfi_fabric_mmap_delete,
	.mmap_find       = vfi_fabric_mmap_find,
	.mmap_put        = vfi_fabric_mmap_put,
	.xfer_create     = vfi_fabric_xfer_create,
	.xfer_delete     = vfi_fabric_xfer_delete,
	.xfer_find       = vfi_fabric_xfer_find,
	.xfer_put        = vfi_fabric_xfer_put,
	.xfer_lose       = vfi_fabric_xfer_lose,
	.sync_create     = vfi_fabric_sync_create,
	.sync_delete     = vfi_fabric_sync_delete,
	.sync_find       = vfi_fabric_sync_find,
	.sync_put        = vfi_fabric_sync_put,
	.sync_wait       = vfi_fabric_sync_wait,
	.sync_send       = vfi_fabric_sync_send,
	.srcs_create     = vfi_fabric_srcs_create,
	.srcs_delete     = vfi_fabric_srcs_delete,
	.src_create      = vfi_fabric_src_create,
	.src_delete      = vfi_fabric_src_delete,
	.src_find        = vfi_fabric_src_find,
	.dsts_create     = vfi_fabric_dsts_create,
	.dsts_delete     = vfi_fabric_dsts_delete,
	.dst_create      = vfi_fabric_dst_create,
	.dst_delete      = vfi_fabric_dst_delete,
	.dst_find        = vfi_fabric_dst_find,
	.bind_find       = vfi_fabric_bind_find,
	.bind_create     = vfi_fabric_bind_create,
	.bind_delete     = vfi_fabric_bind_delete,
	.bind_lose       = vfi_fabric_bind_lose,
	.src_done        = vfi_fabric_src_done,
	.dst_done        = vfi_fabric_dst_done,
	.done            = vfi_fabric_done,
	.src_ready       = vfi_fabric_src_ready,
	.dst_ready       = vfi_fabric_dst_ready,
	.src_events      = vfi_fabric_src_events,
	.dst_events      = vfi_fabric_dst_events,
	.src_ev_delete   = vfi_fabric_src_ev_delete,
	.dst_ev_delete   = vfi_fabric_dst_ev_delete,
	.event_start     = vfi_fabric_event_start,
	.event_find     = vfi_fabric_event_find,
	.event_chain     = vfi_fabric_event_chain,
};

