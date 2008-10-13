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

#define MY_DEBUG      VFI_DBG_OPS | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_OPS | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_OPS | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_drv.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_location.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_src.h>
#include <linux/vfi_dst.h>
#include <linux/vfi_xfer.h>
#include <linux/vfi_bind.h>
#include <linux/vfi_mmap.h>
#include <linux/vfi_events.h>
#include <linux/vfi_event.h>
#include <linux/vfi_sync.h>

#include <linux/device.h>

/**
 * location_create - Creates a location with the given methods and
 * attributes.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int location_create(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_location *new_loc = NULL;
	struct vfi_location *loc;
	struct vfi_desc_param params;
	
	VFI_DEBUG(MY_DEBUG,"%s entered with %s\n",__FUNCTION__,desc);
	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto fail;

	ret = -EINVAL;

	/* Leaf location f.ex node.domain */
	if ( params.location && *params.location ) {
		if ( !(ret = locate_vfi_location(&loc, NULL, &params))) {
			/* 
			   To get the creations right we need to be able to override the 
			   parents location. This is typically necessary when the fabric 
			   is public (no name service used) and we want to create our own 
			   locations (as private). 
			*/
			ret = -EINVAL;
			if (params.ops && params.ops->location_create)
				ret = params.ops->location_create(&new_loc, loc, &params);
			else if (loc && loc->desc.ops && loc->desc.ops->location_create)
				ret = loc->desc.ops->location_create(&new_loc, loc, &params);
			vfi_location_put(loc);
		}
	}

	/* Root location */
	else if (params.ops)
		ret = params.ops->location_create(&new_loc, NULL, &params);

fail:
	if (result) {
		if (ret)
			*size = snprintf(result,*size,"location_create://%s#%llx:%x?result(%d),reply(%s)\n",
				       params.name, params.offset, params.extent, ret,
				       vfi_get_option(&params,"request"));
		else
			*size = snprintf(result,*size,"location_create://%s#%llx:%x?result(%d),reply(%s)\n",
				       new_loc->desc.name, new_loc->desc.offset,new_loc->desc.extent, ret,
				       vfi_get_option(&params,"request"));
	}

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * location_delete - Deletes the named location.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int location_delete(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_location *loc = NULL;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG, "%s %s\n",__FUNCTION__,desc);
	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	if ( params.location && *params.location) {
		if ( !(ret = locate_vfi_location(&loc, NULL, &params) ) ) {
			ret = -EINVAL;
			if ( loc && loc->desc.ops && loc->desc.ops->location_delete ) {
				loc->desc.ops->location_delete(loc, &params);
				ret = 0;
			}
			vfi_location_put(loc);
		}
	}
	else if (params.ops) {
		params.ops->location_delete(NULL,&params);
	}
	else {
		ret = -EINVAL;
		loc = to_vfi_location(kset_find_obj(&vfi_subsys->kset,params.name));
		VFI_DEBUG(MY_DEBUG, "%s kset_find %s gives %p\n", __FUNCTION__, params.name, loc);
		if (loc) {
			ret = 0;
			vfi_location_delete(loc);
			vfi_location_put(loc);
		}
	}
out:
	if (result)
		*size = snprintf(result,*size,"location_delete://%s.%s?result(%d),reply(%s)\n", params.name, params.location,ret, vfi_get_option(&params,"request"));
	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * location_find - Finds the named location.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int location_find(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_location *new_loc = NULL;
	struct vfi_location *loc = NULL;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);
	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	VFI_DEBUG(MY_DEBUG,"%s %s,%s\n",__FUNCTION__,params.name,params.location);

	ret = -EINVAL;
	if (params.location && *params.location ) {
		if ( !(ret = locate_vfi_location(&loc,NULL,&params)) ) {
			if (loc && loc->desc.ops) {
				ret = loc->desc.ops->location_find(&new_loc,loc,&params);
			}
			vfi_location_put(loc);
		}
	}
	else {
		if (params.ops)
			ret = params.ops->location_find(&new_loc,NULL,&params);
		else
			ret = find_vfi_name(&new_loc,NULL, &params);
	}
out:
	if (result) {
		if (ret)
			*size = snprintf(result,*size,"location_find://%s#%llx:%x?result(%d),reply(%s)\n",
				       params.name,params.offset,params.extent,
				       ret, vfi_get_option(&params,"request"));
		else
			*size = snprintf(result,*size,"location_find://%s#%llx:%x?result(%d),reply(%s)\n",
				       new_loc->desc.name,new_loc->desc.offset,new_loc->desc.extent,
				       ret, vfi_get_option(&params,"request"));
	}
	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * location_put - Unfinds the named location.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int location_put(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_location *loc = NULL;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG, "%s %s\n",__FUNCTION__,desc);
	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -EINVAL;

	if (params.location && *params.location) {
		if ( !(ret = locate_vfi_location(&loc,NULL,&params) ) ) {
			if ( loc && loc->desc.ops && loc->desc.ops->location_put ) {
				loc->desc.ops->location_put(loc, &params);
			}
			vfi_location_put(loc);
		}
	}
	else if (params.ops) {
		params.ops->location_put(NULL,&params);
	}
	else {
		ret = -EINVAL;
		loc = to_vfi_location(kset_find_obj(&vfi_subsys->kset,params.name));
		VFI_DEBUG(MY_DEBUG, "%s kset_find %s gives %p\n", __FUNCTION__, params.name, loc);
		if (loc) {
			ret = 0;
			loc->desc.ops->location_put(NULL,&params);
		}
	}
out:
	if (result)
		*size = snprintf(result,*size,"location_put://%s.%s?result(%d),reply(%s)\n",
			       params.name, params.location,
			       ret, vfi_get_option(&params,"request"));
	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}


/**
 * smb_create - Creates an SMB with given attributes at location.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int smb_create(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_smb *smb = NULL;
	struct vfi_location *loc;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = locate_vfi_location(&loc,NULL,&params) ) ) {
		ret = -EINVAL;
		if (loc && loc->desc.ops && loc->desc.ops->smb_create)
			ret = loc->desc.ops->smb_create(&smb,loc, &params);
	}

	vfi_location_put(loc);

out:		
	if (result) {
		if (smb)
			*size = snprintf(result,*size,"smb_create://%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       smb->desc.name, smb->desc.location,smb->desc.offset, smb->desc.extent,
				       ret, vfi_get_option(&params,"request"));
		else 
			*size = snprintf(result,*size,"smb_create://%s.%s?result(%d),reply(%s)\n",
				       params.name, params.location,
				       ret, vfi_get_option(&params,"request"));
	}

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * smb_delete - Deletes named SMB
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int smb_delete(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_smb *smb;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_smb(&smb,&params) ) ) {
		ret = -EINVAL;

		if ( smb && smb->desc.ops && smb->desc.ops->smb_delete ) {
			smb->desc.ops->smb_delete(smb, &params);
			ret = 0;
		}
		vfi_smb_put(smb);
	}

out:
	if (result) 
		*size = snprintf(result,*size,"smb_delete://%s.%s?result(%d),reply(%s)\n",
				 params.name, params.location,ret, vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * smb_find - Finds the named SMB.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int smb_find(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_smb *smb = NULL;
	struct vfi_location *loc;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if (!params.location)  
		goto out;

	if ( !(ret = locate_vfi_location(&loc,NULL,&params)) ) {
		ret = -EINVAL;
		
		if (loc && loc->desc.ops && loc->desc.ops->smb_find)
			ret = loc->desc.ops->smb_find(&smb,loc,&params);
	}

	vfi_location_put(loc);

out:
	if (result) {
		if (smb)
			*size = snprintf(result,*size,"smb_find://%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       smb->desc.name, smb->desc.location,smb->desc.offset, smb->desc.extent, ret, vfi_get_option(&params,"request"));
		else
			*size = snprintf(result,*size,"smb_find://%s.%s?result(%d),reply(%s)\n", params.name, params.location, ret, vfi_get_option(&params,"request"));
	}

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * smb_put - Puts named SMB
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int smb_put(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_smb *smb;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_smb(&smb,&params) ) ) {
		ret = -EINVAL;

		if ( smb && smb->desc.ops && smb->desc.ops->smb_put ) {
			/* put to accomodate for the find */
			vfi_smb_put(smb);
			/* put will delete and make sure it works accross the fabric */
			vfi_smb_put(smb);
			ret = 0;
		}
	}

out:
	if (result) 
		*size = snprintf(result,*size,"smb_put://%s.%s?result(%d),reply(%s)\n",
				 params.name, params.location,ret, vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * smb_lose - Decrement the ref count of only the remote named smb component
 * This method is here to solve the problem of decrementing the ref count
 * of the remote instance of the smb component when the local one is destroyed.
 * This method is private to the driver and should not be called by the user.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int smb_lose(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_smb *smb;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_smb(&smb,&params) ) ) {
		ret = -EINVAL;

		if ( smb && smb->desc.ops && smb->desc.ops->smb_lose ) {
			smb->desc.ops->smb_lose(smb, &params);
			ret = 0;
		}
		/* put to accomodate for the find */
   		vfi_smb_put(smb);
	}

out:
	if (result) 
		*size = snprintf(result,*size,"smb_lose://%s.%s?result(%d),reply(%s)\n",
				 params.name, params.location,ret, vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
* valid_extents - validate any extents quoted in a bind specification
* @x - pointer to bind parameter block
*
* A bind specification has the following general form:
*
*	<xfer>#<xo>:<xe>/<dst-smb>#<do>:<de>=<src-smb>#<so>:<se>
*
* This function validates the extent fields <xe>, <de>, and <se> 
* to ensure, if possible, that all three have the same value.
*
* At least one of <xe>, <de>, and <se> must be non-zero. That allows
* default values to be assigned to unspecified extents by substitution
* of specified values.
*
* All three must have the same value in the end.
*
* Note that this function does NOT validate extents against referenced
* <dst-smb> or <src-smb>: so it would happily pass an extent that is greater
* than either or both, or extents referring to non-existant SMBs.
* 
**/
static int valid_extents(struct vfi_bind_param *x)
{
	/*
	* Fail if <xe> == <de> == <se> == 0
	*
	*/
	if ( x->xfer.extent == 0 && x->dst.extent == 0 && x->src.extent == 0 )
		return 0;

	if (x->dst.extent == 0 && x->src.extent == 0) {
		x->dst.extent = x->src.extent = x->xfer.extent;
		return 1;
	}
	
	if (x->dst.extent == 0)
		x->dst.extent = x->src.extent;

	if (x->src.extent == 0)
		x->src.extent = x->dst.extent;

	if (x->xfer.extent == 0)
		x->xfer.extent = x->dst.extent;

	/*
	* Fail if one or more extents differ from the others.
	*/
	if (x->xfer.extent != x->dst.extent || x->xfer.extent != x->src.extent || x->dst.extent != x->src.extent)
		return 0;

	return 1;
}

/**
 * mmap_create - Create mapping descriptor for mmap
 *
 * @desc:   Null-terminated string with parameters of operation
 * @result: Pointer to buffer to contain result string
 * @size:   Maximum size of result buffer
 *
 * This function services "mmap_create" requests, whose job is to prepare
 * an mmap ticket that can be used to map user virtual memory to
 * all or part of a specified SMB. Ticketing is a scheme that allows
 * SMBs to be mmaped using indirect references to pre-stored "tickets"
 * that have identified the SMB and its page table in advance: something
 * an mmap call can't do. 
 *
 * Offset and extent fields in the command string will be taken 
 * into account when constructing the ticket - although extent
 * is only advisory. Any offset value will resolve to a page offset,
 * and will affect the page table address and table size values
 * written in the ticket. 
 *
 * The function writes the ticket number into the reply string as
 * "reply=<ticket>". Users should use that number as the offset argument
 * in an mmap call to the vfi device. See vfi_cdev.c::vfi_mmap()
 * for what happens next.
 *
 */
static int mmap_create (const char* desc, char* result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_smb *smb = NULL;
	struct vfi_desc_param params;
	struct vfi_mmap *mmap = NULL;
	
	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	if (params.offset % PAGE_SIZE) {
		ret = -EINVAL;
		goto out;
	}

	ret = -ENODEV;

	if ( !(ret = find_vfi_smb(&smb,&params)) ) {
		ret = -EINVAL;
		
		if (smb && smb->desc.ops && smb->desc.ops->mmap_create)
			ret = smb->desc.ops->mmap_create (&mmap,smb, &params);
	}

	vfi_smb_put(smb);

out:		
	if (result) {
		if (mmap) {
			/*
			* Note that because mmap identifiers are huge numbers, write
			* them as hex digits in the response.
			*/
			*size = snprintf(result,*size,"mmap_create://%s.%s?result(%d),reply(%s),mmap_offset(%lx)\n",
				       params.name, params.location, ret, 
				       vfi_get_option(&params,"request"),
				       (unsigned long)mmap_to_ticket(mmap));
		}
		else {
			*size = snprintf(result,*size,"mmap_create://%s.%s?result(%d),reply(%s)\n", 
				       params.name, params.location,ret, vfi_get_option(&params,"request"));
		}
	}
	
	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

static int mmap_find (const char* desc, char* result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_smb *smb = NULL;
	struct vfi_mmap *mmap = NULL;
	struct vfi_desc_param params;
	
	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = find_vfi_smb(&smb,&params);
	if (ret)
		goto out;

	ret = -EINVAL;

	if (smb && smb->desc.ops && smb->desc.ops->mmap_find)
		ret = smb->desc.ops->mmap_find(&mmap,smb,&params);

	vfi_smb_put(smb);

out:		
	if (result) {
		*size = snprintf(result,*size,"mmap_find://%s.%s?result(%d),reply(%s),mmap_offset(%lx)\n",
				 params.name, params.location, ret, vfi_get_option(&params,"request"), (unsigned long)mmap_to_ticket(mmap));
	}
	
	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

static int mmap_delete (const char* desc, char* result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_smb *smb = NULL;
	struct vfi_mmap *mmap = NULL;
	struct vfi_desc_param params;
	
	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	if (params.offset % PAGE_SIZE) {
		ret = -EINVAL;
		goto out;
	}

	ret = find_vfi_smb(&smb,&params);
	if (ret)
		goto out;
		
	ret = -EINVAL;

	if (smb && smb->desc.ops && smb->desc.ops->mmap_find)
		ret = smb->desc.ops->mmap_find(&mmap,smb,&params);

	if (ret)
		goto fail;

	mmap->desc.ops->mmap_delete(mmap,&params);
	mmap->desc.ops->mmap_put(mmap,&params);
fail:
	vfi_smb_put(smb);
out:		
	if (result) {
		*size = snprintf(result,*size,"mmap_delete://%s.%s?result(%d),reply(%s)\n", params.name, params.location, ret, vfi_get_option(&params,"request"));
	}
	
	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

static int mmap_put (const char* desc, char* result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_smb *smb = NULL;
	struct vfi_mmap *mmap = NULL;
	struct vfi_desc_param params;
	
	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	if (params.offset % PAGE_SIZE) {
		ret = -EINVAL;
		goto out;
	}

	ret = find_vfi_smb(&smb,&params);
	if (ret)
		goto out;
		
	ret = -EINVAL;

	if (smb && smb->desc.ops && smb->desc.ops->mmap_find)
		ret = smb->desc.ops->mmap_find(&mmap,smb,&params);

	if (ret)
		goto fail;

	mmap->desc.ops->mmap_put(mmap,&params);
	mmap->desc.ops->mmap_put(mmap,&params);
fail:
	vfi_smb_put(smb);
out:		
	if (result) {
		*size = snprintf(result,*size,"mmap_put://%s.%s?result(%d),reply(%s)\n", params.name, params.location, ret, vfi_get_option(&params,"request"));
	}
	
	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}


/**
 * xfer_create - Creates the named transfer bind component.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int xfer_create(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_xfer *xfer = NULL;
	struct vfi_location *location;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = locate_vfi_location(&location,NULL,&params) ) ) {
		ret = -EINVAL;
		
		if (location && location->desc.ops && location->desc.ops->xfer_create)
			ret = location->desc.ops->xfer_create(&xfer,location, &params);
	}

	vfi_location_put(location);

out:		
	if (result)
		*size = snprintf(result,*size,"xfer_create://%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.name, params.location,params.offset, params.extent,
			       ret,vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * xfer_delete - Deletes the named transfer
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int xfer_delete(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_xfer *xfer;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_xfer(&xfer, &params) ) ) {
		ret = -EINVAL;

		if ( xfer && xfer->desc.ops && xfer->desc.ops->xfer_delete ) {
			xfer->desc.ops->xfer_delete(xfer, &params);
			ret = 0;
		}
		vfi_xfer_put(xfer);
	}

out:
	if (result) 
		*size = snprintf(result,*size,"xfer_delete://%s.%s?result(%d),reply(%s)\n",
			       params.name, params.location,ret, vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * xfer_find - Finds the named transfer.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int xfer_find(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_xfer *xfer = NULL;
	struct vfi_location *location;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = locate_vfi_location(&location,NULL,&params)) ) {
		ret = -EINVAL;
		if (location && location->desc.ops && location->desc.ops->xfer_find)
			ret = location->desc.ops->xfer_find(&xfer,location,&params);
	}

	vfi_location_put(location);

out:
	if (result) {
		if (xfer)
			*size = snprintf(result,*size,"xfer_find://%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.name, params.location,xfer->desc.offset, xfer->desc.extent,
				       ret,vfi_get_option(&params,"request"));
		else
			*size = snprintf(result,*size,"xfer_find://%s.%s?result(%d),reply(%s)\n",
				       params.name, params.location,
				       ret,vfi_get_option(&params,"request"));
	}

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * xfer_put - Puts the named transfer
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int xfer_put(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_xfer *xfer;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_xfer(&xfer, &params) ) ) {
		ret = -EINVAL;

		if ( xfer && xfer->desc.ops && xfer->desc.ops->xfer_put ) {
			xfer->desc.ops->xfer_put(xfer, &params);
			ret = 0;
		}
		vfi_xfer_put(xfer);
	}

out:
	if (result) 
		*size = snprintf(result,*size,"xfer_put://%s.%s?result(%d),reply(%s)\n",
			       params.name, params.location,ret, vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * xfer_lose - Decrement the ref count of only the remote named xfer component
 * This method is here to solve the problem of decrementing the ref count
 * of the remote instance of the xfer component when the local one is destroyed.
 * This method is private to the driver and should not be called by the user.
 *
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int xfer_lose(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_xfer *xfer;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_xfer(&xfer, &params) ) ) {
		ret = -EINVAL;

		if ( xfer && xfer->desc.ops && xfer->desc.ops->xfer_lose ) {
			xfer->desc.ops->xfer_lose(xfer, &params);
			ret = 0;
		}
		vfi_xfer_put(xfer);
	}

out:
	if (result) 
		*size = snprintf(result,*size,"xfer_lose://%s.%s?result(%d),reply(%s)\n",
			       params.name, params.location,ret, vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * sync_create - Creates the named transfer bind component.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int sync_create(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_sync *sync = NULL;
	struct vfi_location *location;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = locate_vfi_location(&location,NULL,&params) ) ) {
		ret = -EINVAL;
		
		if (location && location->desc.ops && location->desc.ops->sync_create)
			ret = location->desc.ops->sync_create(&sync,location, &params);
	}

	vfi_location_put(location);

out:		
	if (result)
		*size = snprintf(result,*size,"sync_create://%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.name, params.location,params.offset, params.extent,
			       ret,vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * sync_delete - Deletes the named transfer
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int sync_delete(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_sync *sync;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_sync(&sync,&params) ) ) {
		ret = -EINVAL;
		if ( sync && sync->desc.ops && sync->desc.ops->sync_delete ) {
			sync->desc.ops->sync_delete(sync, &params);
			ret = 0;
		}
	}

	vfi_sync_put(sync);

out:
	if (result) 
		*size = snprintf(result,*size,"sync_delete://%s.%s?result(%d),reply(%s)\n",
			       params.name, params.location,ret, vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * sync_find - Finds the named transfer.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int sync_find(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_sync *sync = NULL;
	struct vfi_location *location;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = locate_vfi_location(&location,NULL,&params)) ) {
		ret = -EINVAL;
		if (location && location->desc.ops && location->desc.ops->sync_find)
			ret = location->desc.ops->sync_find(&sync,location,&params);
	}

	vfi_location_put(location);

out:
	if (result) {
		if (sync)
			*size = snprintf(result,*size,"sync_find://%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.name, params.location,sync->desc.offset, sync->desc.extent,
				       ret,vfi_get_option(&params,"request"));
		else
			*size = snprintf(result,*size,"sync_find://%s.%s?result(%d),reply(%s)\n",
				       params.name, params.location,
				       ret,vfi_get_option(&params,"request"));
	}

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * sync_put - Puts the named sync.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int sync_put(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_sync *sync = NULL;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_sync(&sync,&params)) ) {
		if (sync && sync->desc.ops && sync->desc.ops->sync_put)
			sync->desc.ops->sync_put(sync,&params);
	}

	vfi_sync_put(sync);

out:
	if (result) {
		if (sync)
			*size = snprintf(result,*size,"sync_put://%s.%s?result(%d),reply(%s)\n",
				       params.name, params.location,
				       ret,vfi_get_option(&params,"request"));
		else
			*size = snprintf(result,*size,"sync_put://%s.%s?result(%d),reply(%s)\n",
				       params.name, params.location,
				       ret,vfi_get_option(&params,"request"));
	}

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * sync_send - Sends the named sync component.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int sync_send(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_sync *sync = NULL;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_sync(&sync,&params) ) ) {
		ret = -EINVAL;
		
		if (sync && sync->desc.ops && sync->desc.ops->sync_send)
			ret = sync->desc.ops->sync_send(sync,&params);
	}

	vfi_sync_put(sync);

out:		
	if (result)
		*size = snprintf(result,*size,"sync_send://%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.name, params.location,params.offset, params.extent,
			       ret,vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * sync_wait - Waits the sync component.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int sync_wait(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_sync *sync = NULL;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_sync(&sync,&params) ) ) {
		ret = -EINVAL;
		
		if (sync && sync->desc.ops && sync->desc.ops->sync_wait)
			ret = sync->desc.ops->sync_wait(sync,&params);
	}

	vfi_sync_put(sync);

out:		
	if (result)
		*size = snprintf(result,*size,"sync_wait://%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.name, params.location,params.offset, params.extent,
			       ret,vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * bind_create - Creates a bind component.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 *
 * A bind is specified by three terms:
 *
 *	<xfer-spec>/<dst-spec>=<src-spec>
 *
 * <xfer-spec> identifies the transfer that the bind will belong to.
 * <dst-spec> identifies the destination SMB fragment that data is to be copied to
 * <src-spec> identifies the source SMB fragment that data is to be copied from.
 *
 *
 */
static int bind_create(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_xfer *xfer = NULL;
	struct vfi_bind *bind = NULL;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	/*
	* Parse <bind-spec> into <xfer-spec>, <dst-spec>, 
	* and <src-spec>.
	*
	*/
	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	ret = -EINVAL;

	/*
	* Elementary extent validation to ensure all three extents
	* are equal in [non-zero] value. Will substitute for zero
	* if possible. No validation of actual extents against the
	* SMBs they refer to.
	*/
	if ( !valid_extents(&params) )
		goto out;

	ret = -ENODEV;

	/*
	* Try to find the xfer object in the tree. This may not be successful,
	* in which case we will fail to retrieve a bind pointer, and will consequently
	* return a failure reply.
	*
	* Sometimes that just needs to be spelled out.
	*/
	if ( !(ret = find_vfi_xfer(&xfer,&params.xfer) ) ) {

		ret = -EINVAL;
		if (xfer && xfer->desc.ops && xfer->desc.ops->bind_create)
			ret = xfer->desc.ops->bind_create(&bind,xfer, &params);
	}

	vfi_xfer_put(xfer);

out:		
	if (result) {
		if (bind)
			*size = snprintf(result,*size,"bind_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       bind->desc.xfer.name, bind->desc.xfer.location, bind->desc.xfer.offset, bind->desc.xfer.extent,
				       bind->desc.dst.name,  bind->desc.dst.location,  bind->desc.dst.offset,  bind->desc.dst.extent,
				       bind->desc.src.name,  bind->desc.src.location,  bind->desc.src.offset,  bind->desc.src.extent,
				       ret,vfi_get_option(&params.src,"request"));
		else
			*size = snprintf(result,*size,"bind_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name, params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name,params.dst.location, params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,vfi_get_option(&params.src,"request"));
	}

	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

/**
 * bind_delete - Deletes a named bind component.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 *
 * Returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 *
 * This function handles commands of the form:
 *
 *    bind_delete://<xfer>#<xo>
 *
 *	<xfer> - name of xfer the binding belongs to
 *	<xo>   - Offset to the start of the bind within the transfer
 *
 * bind_delete must run on the Xfer agent. This function will find the named xfer 
 * object in the local tree, and will then invoke its Xfer agent bind_delete operation.
 *
 */
static int bind_delete(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_xfer *xfer = NULL;
	struct vfi_desc_param params;
	
	VFI_DEBUG (MY_DEBUG, "%s: \"%s\"\n", __FUNCTION__, desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;
	
	if ( !(ret = find_vfi_xfer(&xfer, &params) ) ) {
		ret = -EINVAL;

		if ( xfer && xfer->desc.ops && xfer->desc.ops->bind_delete ) {
			xfer->desc.ops->bind_delete(xfer, &params);
			ret = 0;
		}
		vfi_xfer_put (xfer);
	}

out:
	if (result) 
		*size = snprintf(result,*size,"bind_delete://%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.name, params.location,params.offset, params.extent,
			       ret,vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * bind_find - Find named bind.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int bind_find(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_xfer *xfer = NULL;
	struct vfi_bind *bind = NULL;
	struct vfi_desc_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_xfer(&xfer,&params)) ) {
		ret = -EINVAL;
		if (xfer && xfer->desc.ops && xfer->desc.ops->bind_find)
			ret = xfer->desc.ops->bind_find(&bind,xfer,&params);
	}

	vfi_xfer_put(xfer);

out:
	if (result) {
		if (bind)
			*size = snprintf(result,*size,"bind_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       bind->desc.xfer.name, bind->desc.xfer.location,bind->desc.xfer.offset, bind->desc.xfer.extent,
				       bind->desc.dst.name, bind->desc.dst.location,bind->desc.dst.offset, bind->desc.dst.extent,
				       bind->desc.src.name, bind->desc.src.location,bind->desc.src.offset, bind->desc.src.extent,
				       ret,vfi_get_option(&params,"request"));
		else
			*size = snprintf(result,*size,"bind_find://%s.%s#%llx:%x/%s.%s=%s.%s#?result(%d),reply(%s)\n",
				       params.name, params.location, params.offset, params.extent,
				       params.name, params.location,
				       params.name, params.location,
				       ret,vfi_get_option(&params,"request"));
	}

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * bind_lose - Decrement the ref count of only the remote named bind component
 * This method is here to solve the problem of decrementing the ref count
 * of the remote instance of the bind component when the local one is destroyed.
 * This method is private to the driver and should not be called by the user.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 *
 * Returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 *
 * This function handles commands of the form:
 *
 *    bind_lose://<xfer>#<xo>
 *
 *	<xfer> - name of xfer the binding belongs to
 *	<xo>   - Offset to the start of the bind within the transfer
 *
 * bind_lose must run on the Xfer agent. This function will find the named xfer 
 * object in the local tree, and will then invoke its Xfer agent bind_lose operation.
 *
 */
static int bind_lose(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_xfer *xfer = NULL;
	struct vfi_desc_param params;
	
	VFI_DEBUG (MY_DEBUG, "%s: \"%s\"\n", __FUNCTION__, desc);

	if ( (ret = vfi_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;
	
	if ( !(ret = find_vfi_xfer(&xfer, &params) ) ) {
		ret = -EINVAL;

		if ( xfer && xfer->desc.ops && xfer->desc.ops->bind_lose ) {
			xfer->desc.ops->bind_lose(xfer, &params);
			ret = 0;
		}
		vfi_xfer_put (xfer);
	}

out:
	if (result) 
		*size = snprintf(result,*size,"bind_lose://%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.name, params.location,params.offset, params.extent,
			       ret,vfi_get_option(&params,"request"));

	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

/**
 * dst_create - Creates named destination component
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int dst_create(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_dst *dst = NULL;
	struct vfi_bind *bind = NULL;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_bind(&bind,&params.xfer) ) ) {
		ret = -EINVAL;
		if (bind && bind->desc.xfer.ops && bind->desc.xfer.ops->dst_create)
			ret = bind->desc.xfer.ops->dst_create(&dst,bind, &params);
	}

	VFI_KTRACE ("<*** %s bind put after dst_create opcall ***>\n", __func__);
	vfi_bind_put(bind);

out:		
	if (result) {
		if (dst)
			*size = snprintf(result,*size,"dst_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_name(%s),result(%d),reply(%s)\n",
				       dst->desc.xfer.name,dst->desc.xfer.location,dst->desc.xfer.offset,dst->desc.xfer.extent,
				       dst->desc.dst.name,dst->desc.dst.location,dst->desc.dst.offset,dst->desc.dst.extent,
				       dst->desc.src.name,dst->desc.src.location,dst->desc.src.offset,dst->desc.src.extent,
				       vfi_get_option(&params.src,"event_name"),ret,vfi_get_option(&params.src,"request"));
		else
			*size = snprintf(result,*size,"dst_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_name(%s),result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset,params.xfer.extent,
				       params.dst.name,params.dst.location,params.dst.offset,params.dst.extent,
				       params.src.name,params.src.location,params.src.offset,params.src.extent,
				       vfi_get_option(&params.src,"event_name"),ret,vfi_get_option(&params.src,"request"));
	}

	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

/**
 * dst_delete - Deletes named destination component.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 *
 * The dst_delete command is intended to be run as an integral part of a 
 * larger bind_delete operation: it is NOT meant for casual use by applications.
 *
 * dst_delete runs on the <xfer> component of a <bind> that is being deleted.
 * It is invoked during the dsts_delete action, which runs on the <dst> component of
 * the <bind>. 
 *
 */
static int dst_delete(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_bind *bind = NULL;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;
	if ( !(ret = find_vfi_bind(&bind,&params.xfer) ) ) {
		ret = -EINVAL;
		if ( bind && bind->desc.xfer.ops && bind->desc.xfer.ops->dst_delete ) {
			bind->desc.xfer.ops->dst_delete(bind, &params);
			ret = 0;
		}
	}

	vfi_bind_put(bind);

out:
	if (result)
		*size = snprintf(result,*size,"dst_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset,params.xfer.extent,
				       params.dst.name,params.dst.location,params.dst.offset,params.dst.extent,
				       params.src.name,params.src.location,params.src.offset,params.src.extent,
				       ret,vfi_get_option(&params.src,"request"));

	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

/**
 * dst_find - Find named destination component.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int dst_find(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_dst *dst = NULL;
	struct vfi_bind *bind = NULL;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_bind(&bind,&params.xfer)) ) {
		ret = -EINVAL;
		if (bind && bind->desc.dst.ops && bind->desc.dst.ops->dst_find)
			ret = bind->desc.dst.ops->dst_find(&dst,bind,&params);
	}

	VFI_KTRACE ("<*** %s bind put after dst_find opcall ***>\n", __func__);
	vfi_bind_put(bind);

out:
	if (result) {
		if (dst)
			*size = snprintf(result,*size,"dst_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       dst->desc.xfer.name,dst->desc.xfer.location,dst->desc.xfer.offset, dst->desc.xfer.extent,
				       dst->desc.dst.name, dst->desc.dst.location,dst->desc.dst.offset, dst->desc.dst.extent,
				       dst->desc.src.name, dst->desc.src.location,dst->desc.src.offset, dst->desc.src.extent,
				       ret,vfi_get_option(&params.src,"request"));
		else
			*size = snprintf(result,*size,"dst_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,vfi_get_option(&params.src,"request"));
	}

	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

/**
 * src_create - Creates named source component
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 *
 * This operation is NOT intended to be invoked directly by an application, 
 * but rather from within the driver during bind_create.
 */
static int src_create(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_src *src = NULL;
	struct vfi_dst *dst = NULL;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	ret = find_vfi_dst(&dst,&params);

	if (ret || dst == NULL)
		goto out;

	ret = -EINVAL;

	if (dst->desc.xfer.ops && dst->desc.xfer.ops->src_create)
		ret = dst->desc.xfer.ops->src_create(&src,dst, &params);


	vfi_dst_put(dst);

out:		
	if (result) {
		if (src)
			*size = snprintf(result,*size,"src_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       src->desc.xfer.name, src->desc.xfer.location,src->desc.xfer.offset, src->desc.xfer.extent,
				       src->desc.dst.name, src->desc.dst.location,src->desc.dst.offset, src->desc.dst.extent,
				       src->desc.src.name, src->desc.src.location,src->desc.src.offset, src->desc.src.extent,
				       ret,vfi_get_option(&params.src,"request"));
		else
			*size = snprintf(result,*size,"src_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,vfi_get_option(&params.src,"request"));
	}

	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

/**
 * src_delete - Deletes named source component.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int src_delete(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_dst *dst = NULL;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_dst(&dst, &params) ) ) {
		ret = -EINVAL;
		if ( dst && dst->desc.xfer.ops && dst->desc.xfer.ops->src_delete ) {
			dst->desc.xfer.ops->src_delete(dst, &params);
			ret = 0;
		}
		vfi_dst_put(dst);
	}


out:
	if (result)
		*size = snprintf(result,*size,"src_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,vfi_get_option(&params.src,"request"));

	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

/**
 * src_find - Finds named source component.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int src_find(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_src *src = NULL;
	struct vfi_dst *dst = NULL;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_dst(&dst,&params)) ) {
		ret = -EINVAL;
		if (dst && dst->desc.dst.ops && dst->desc.dst.ops->src_find) {
			ret = dst->desc.dst.ops->src_find(&src,dst,&params);
			ret = 0;
		}
	}

	vfi_dst_put(dst);

out:
	if (result) {
		if (src)
			*size = snprintf(result,*size,"src_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       src->desc.xfer.name, src->desc.xfer.location,src->desc.xfer.offset, src->desc.xfer.extent,
				       src->desc.dst.name, src->desc.dst.location,src->desc.dst.offset, src->desc.dst.extent,
				       src->desc.src.name, src->desc.src.location,src->desc.src.offset, src->desc.src.extent,
				       ret,vfi_get_option(&params.src,"request"));
		else
			*size = snprintf(result,*size,"src_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,vfi_get_option(&params.src,"request"));
	}

	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

/**
 * srcs_create - Creates set holder for sources.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 *
 * This function, which runs as part of bind creation, is responsible
 * for creating the set of bind <src> fragments needed to satisfy a given
 * <dst> fragment.
 *
 * To elaborate: a <bind> defines a transfer of <extent> bytes from a
 * <src> SMB to a <dst> SMB. The <extent> will be sub-divided first into
 * smaller <dst> fragments, whose individual size depends on the page size
 * at the recipient site, and then for each of those, into <src> fragments
 * chosen to suit page size at the source site. Thus a single <bind> flowers
 * into multiple <dsts>, and each <dst> into multiple <srcs>.
 *
 * This operation, srcs_create, decides upon the number and size of <src> 
 * fragments that need to feed a single <dst> fragment. It will create a 
 * <srcs> container, and cause the creation of the required <src> set.
 *
 * The creation of <srcs> will always run on the bind <src> agent, though
 * creation of <src> fragments (and pretty much everything else) runs on
 * the <xfer> agent.
 *
 */
static int srcs_create(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_srcs *srcs = NULL;
	struct vfi_dst *dst = NULL;
	struct vfi_bind *bind = NULL;
	int event_id = -1;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;
	
	if ( !(ret = find_vfi_dst(&dst,&params) ) ) {
		ret = -EINVAL;

		if (dst && dst->desc.src.ops && dst->desc.src.ops->srcs_create)
			ret = dst->desc.src.ops->srcs_create(&srcs,dst, &params);

		bind = vfi_dst_parent(dst);
		event_id = bind->src_done_event->event_id;

		/*
		* TJA> Remove superfluous bind put.
		*
		* This put is unnecessary, and damaging - it causes an imbalance
		* in The Force that is noticed at delete time.
		*/
//		printk ("<*** %s bind put after srcs_create opcall ***>\n", __func__);
//		vfi_bind_put(bind);
	}

	vfi_dst_put(dst);

out:		
	if (result)
		*size = snprintf(result,*size,"srcs_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_id(%d),result(%d),reply(%s)\n",
			       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
			       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
			       params.src.name, params.src.location,params.src.offset, params.src.extent,event_id,
			       ret,vfi_get_option(&params.src,"request"));
	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

/**
 * srcs_delete - Deletes source set.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 *
 * This functions deletes the set of source fragments belonging to a 
 * specific bind destination fragment.
 *
 * The <srcs> component of a given bind is one of only two ONLY parts of 
 * a bind that are not "owned" by the <xfer> agent, and the only part that
 * is owned by the <src> agent. The individual <src> fragments slung beneath
 * <srcs> are themselves owned by the <xfer> agent, as will be the <dst> parent
 * fragment.
 *
 * This makes things a little tricky when <xfer> != <src>, since all higher-level
 * [and lower-level] objects in the bind hierarchy will only exist in the local
 * tree as proxies, created on-the-fly during srcs_create.
 *
 */
static int srcs_delete(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_dst *dst = NULL;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( !(ret = find_vfi_dst(&dst,&params) ) ) {
		ret = -EINVAL;
		if ( dst && dst->desc.src.ops && dst->desc.src.ops->srcs_delete  ) {
			dst->desc.src.ops->srcs_delete(dst, &params);
			ret = 0;
		}
		vfi_dst_put(dst);
	}

out:
	if (result)
		*size = snprintf(result,*size,"srcs_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,vfi_get_option(&params.src,"request"));

	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

/**
 * srcs_find - Find sources
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int srcs_find(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_srcs *srcs = NULL;
	struct vfi_dst *dst = NULL;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	if ( !(ret = find_vfi_dst(&dst,&params)) ) {
		ret = -EINVAL;
		if (dst && dst->desc.dst.ops && dst->desc.dst.ops->srcs_find)
			ret = dst->desc.dst.ops->srcs_find(&srcs,dst,&params);
		vfi_dst_put(dst);
	}


out:
	if (result)
		*size = snprintf(result,*size,"srcs_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
			       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
			       params.src.name, params.src.location,params.src.offset, params.src.extent,
			       ret,vfi_get_option(&params.src,"request"));
	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}
/**
 * dsts_create - Creates set holder for destinations.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 *
 * A "dsts_create" command is not issued directly by users - or, at least, should not be - 
 * but rather as an integral part of bind_create: specifically the part that creates the
 * bind destination sub-tree.
 *
 * Now, when we reach here all we can know is that the Xfer agent - which is presumably
 * some other device on the network (or we wouldn't be here) has created the following 
 * components of a bind:
 *
 * <loc>			The location, created separately
 *     xfers/
 *         <xfer>		The transfer, created separately
 *             binds/
 *             ...		Other binds we do not care about
 *                 #<xo>:<xe>	The header for OUR bind, specifying offset and extent within the xfer.
 *
 * It is certain that we, here, will not know of the bind header. Our job is to create a 
 * local stub and a subtree for it, beginning with <dsts>, a list of <dst>, and ultimately 
 * <srcs> and a list of <src> - although source elements will be created at the <src> site.
 *
 */
static int dsts_create(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_dsts *dsts = NULL;
	struct vfi_bind *bind = NULL;
	struct vfi_bind_param params;
	int event_id = -1;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	/*
	* Parse the bind specification contained in the instruction into its
	* <xfer>, <dst>, and <src> components.
	*
	*/
	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;
	
	/*
	* Find the bind object in our tree. If the <xfer> agent is local,
	* it will already be there. If the <xfer> agent is remote, then
	* we will create a stub for it - provided it exists at the xfer site.
	*
	*/
	if ( !(ret = find_vfi_bind(&bind,&params.xfer) ) ) {
		ret = -EINVAL;

		/*
		* With the bind and its components properly identified, 
		* invoke the dsts_create function at the <dst> site.
		*
		*/
		if (bind && bind->desc.dst.ops && bind->desc.dst.ops->dsts_create)
			ret = bind->desc.dst.ops->dsts_create(&dsts,bind, &params);

		event_id = bind->dst_done_event->event_id;
	}
	VFI_KTRACE ("<*** %s bind put after dsts_create opcall ***>\n", __func__);
	vfi_bind_put(bind);

out:		
	if (result)
		*size = snprintf(result,*size,"dsts_create://%s.%s#%llx:%x/%s.%s#%llx:%x?event_id(%d)=%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
			       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
			       event_id,
			       params.src.name, params.src.location,params.src.offset, params.src.extent,
			       ret,vfi_get_option(&params.src,"request"));
	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

/**
 * dsts_delete - Deletes destination set.
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 *
 * dsts_delete is invoked as part of bind_delete, where it is expressly
 * delivered to the <dst> agent of a bind being deleted.
 *
 */
static int dsts_delete(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_bind *bind = NULL;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	if ( !(ret = find_vfi_bind(&bind,&params.xfer) ) ) {
		ret = -EINVAL;
		if ( bind && bind->desc.dst.ops && bind->desc.dst.ops->dsts_delete ) {
			bind->desc.dst.ops->dsts_delete(bind, &params);
			ret = 0;
		}
		
		vfi_bind_put (bind);
	}

out:
	if (result)
		*size = snprintf(result,*size,"dsts_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,vfi_get_option(&params.src,"request"));

	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

/**
 * dsts_find - Find sources
 *
 * @desc: Null terminated string with parameters of operation
 * @result:Pointer to buffer to hold result string
 * @size: Maximum size of result buffer.
 * returns the number of characters written into result (not including
 * terminating null) or negative if an error.
 * Passing a null result pointer is valid if you only need the success
 * or failure return code.
 */
static int dsts_find(const char *desc, char *result, int *size)
{
	int ret = -ENOMEM;
	struct vfi_dsts *dsts = NULL;
	struct vfi_bind *bind = NULL;
	struct vfi_bind_param params;

	VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = vfi_parse_bind(&params, desc)) )
		goto out;

	if ( !(ret = find_vfi_bind(&bind,&params.xfer)) ) {
		ret = -EINVAL;
		if (bind && bind->desc.dst.ops && bind->desc.dst.ops->dsts_find)
			ret = bind->desc.dst.ops->dsts_find(&dsts,bind,&params);
	}

	VFI_KTRACE ("<*** %s bind put after dsts_find opcall ***>\n", __func__);
	vfi_bind_put(bind);

out:
	if (result)
		*size = snprintf(result,*size,"dsts_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
			       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
			       params.src.name, params.src.location,params.src.offset, params.src.extent,
			       ret,vfi_get_option(&params.src,"request"));
	vfi_clean_bind(&params);

	return VFI_RESULT(ret);
}

static int event_start(const char *desc, char *result, int *size)
{
	struct vfi_desc_param params;
	struct vfi_location *loc;
	int ret = -EINVAL;

	if ( (ret = vfi_parse_desc(&params,desc)) ) {
		goto out;
	}

	if ( params.location && *params.location ) {
		if ( !(ret = locate_vfi_location(&loc,NULL,&params))) {
			if (loc && loc->desc.ops && loc->desc.ops->event_start) {
				ret = loc->desc.ops->event_start(loc,&params);
			}
			vfi_location_put(loc);
		}
	}
	else if (params.ops) {
		ret = params.ops->event_start(NULL,&params);
	}

out:
	if (result)
		*size = snprintf(result,*size,"event_start://%s.%s?result(%d),reply(%s)\n",
			       params.name,params.location,
			       ret,vfi_get_option(&params,"request"));
	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

static int event_find(const char *desc, char *result, int *size)
{
	struct vfi_desc_param params;
	struct vfi_location *loc;
	int ret = -EINVAL;

	if ( (ret = vfi_parse_desc(&params,desc)) ) {
		goto out;
	}

	if ( params.location && *params.location ) {
		if ( !(ret = locate_vfi_location(&loc,NULL,&params))) {
			if (loc && loc->desc.ops && loc->desc.ops->event_find) {
				ret = loc->desc.ops->event_find(loc,&params);
			}
			vfi_location_put(loc);
		}
	}
	else if (params.ops) {
		ret = params.ops->event_find(NULL,&params);
	}

out:
	if (result)
		*size = snprintf(result,*size,"event_find://%s.%s?result(%d),reply(%s)\n",
			       params.name,params.location,
			       ret,vfi_get_option(&params,"request"));
	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

static int event_chain(const char *desc, char *result, int *size)
{
	struct vfi_desc_param params;
	struct vfi_location *loc;
	int ret = -EINVAL;

	VFI_DEBUG (MY_DEBUG,
		     "#### %s entered with desc = %s, result=%p, size=%d\n",
		     __FUNCTION__,desc,result,*size);


	if ( (ret = vfi_parse_desc(&params,desc)) ) {
		goto out;
	}

	if ( params.location && *params.location ) {
		if ( !(ret = locate_vfi_location(&loc,NULL,&params))) {
			if (loc && loc->desc.ops && loc->desc.ops->event_chain) {
				ret = loc->desc.ops->event_chain(loc,&params);
			}
			vfi_location_put(loc);
		}
	}
	else if (params.ops) {
		ret = params.ops->event_chain(NULL,&params);
	}

out:
	if (result)
		*size = snprintf(result,*size,"event_chain://%s.%s?result(%d),reply(%s)\n",
			       params.name,params.location,
			       ret,vfi_get_option(&params,"request"));
	vfi_clean_desc(&params);

	return VFI_RESULT(ret);
}

#define MAX_OP_LEN 15		/* length of location_create */
static struct ops {
	char *cmd;
	int (*f)(const char *, char *, int *);
} ops [] = {
	{"event_start",event_start},
	{"event_find",event_find},
	{"event_chain",event_chain},

	{"location_create", location_create},
	{"location_delete", location_delete},
	{"location_find", location_find},
	{"location_put", location_put},

	{"smb_create", smb_create},
	{"smb_delete", smb_delete},
	{"smb_find", smb_find},
	{"smb_put", smb_put},
	{"smb_lose", smb_lose},

	{"mmap_create", mmap_create}, 
	{"mmap_delete",mmap_delete},
	{"mmap_find", mmap_find}, 
	{"mmap_put",mmap_put},

	{"sync_create", sync_create},
	{"sync_delete", sync_delete},
	{"sync_find", sync_find},
	{"sync_put", sync_put},
	{"sync_send", sync_send},
	{"sync_wait", sync_wait},

	{"xfer_create", xfer_create},
	{"xfer_delete", xfer_delete},
	{"xfer_find", xfer_find},
	{"xfer_put", xfer_put},
	{"xfer_lose", xfer_lose},

	{"bind_create", bind_create},
	{"bind_delete", bind_delete},
	{"bind_find", bind_find},
	{"bind_lose", bind_lose},

	{"dsts_create", dsts_create},
	{"dsts_delete", dsts_delete},
	{"dsts_find", dsts_find},

	{"dst_create", dst_create},
	{"dst_delete", dst_delete},
	{"dst_find", dst_find},

	{"srcs_create", srcs_create},
	{"srcs_delete", srcs_delete},
	{"srcs_find", srcs_find},

	{"src_create", src_create},
	{"src_delete", src_delete},
	{"src_find", src_find},

	{0,0},
};

/**
 *do_operation - Interprets and executes a descriptor string
 *@cmd: A string of the form op_name://name.loc...
 *@result: Pointer to a buffer to hold a string result description
 *@size: The size of the result buffer available for use
 *
 * Looks up the function corresponding to op_name and calls it with the
 * rest of the string, name.loc... and the result buffer and
 * size. Return value is the number of characters written into result
 * or negative if there is an error.
 */

int do_operation(const char *cmd, char *result, int *size)
{
	static int nested = 0;
	int ret = -EINVAL;
	char *sp1;
	char test[MAX_OP_LEN + 1];
	int toklen;
	int found = 0;
	int this_nested = ++nested;

	VFI_DEBUG (MY_DEBUG,"#### %s (%d) entered with %s, result=%p, size=%d\n",__FUNCTION__,this_nested,cmd,result, *size);

	if ( (sp1 = strstr(cmd,"://")) ) {
		struct ops *op = &ops[0];
		toklen = sp1 - cmd;
		if (toklen > MAX_OP_LEN)
			goto out;

		strncpy(test,cmd,toklen);
		test[toklen] = '\0';
		while (op->f)
			if (!strcmp(test,op->cmd)) {
				ret = op->f(sp1+3,result,size);
				found = 1;
				break;
			}
			else
				op++;
	}
out:
	if (!found) {
		int reply = 0;
		char *request = strstr(cmd,"request");
		if (request)
			sscanf(request,"request(%x)",&reply);
		*size = snprintf(result,*size,"%s%cresult(10101),reply(%x)\n" , cmd, request ? ',' : '?',reply);
	}

	nested--;
	VFI_DEBUG (MY_DEBUG, "#### [ done_operation (%d) \"%s\" ]\n", this_nested, result);
	return VFI_RESULT(ret);
}

