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

#define MY_DEBUG      RDDMA_DBG_OPS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_OPS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_drv.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_location.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_smb.h>
#include <linux/rddma_src.h>
#include <linux/rddma_dst.h>
#include <linux/rddma_xfer.h>
#include <linux/rddma_bind.h>
#include <linux/rddma_mmap.h>
#include <linux/rddma_events.h>
#include <linux/rddma_event.h>

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
static int location_create(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_location *new_loc = NULL;
	struct rddma_location *loc;
	struct rddma_desc_param params;
	
	RDDMA_DEBUG(MY_DEBUG,"%s entered with %s\n",__FUNCTION__,desc);
	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto fail;

	ret = -EINVAL;

	if ( params.location && *params.location ) {
		if ( (loc = locate_rddma_location(NULL,&params))) {
			if (loc && loc->desc.ops && loc->desc.ops->location_create) {
				ret = (new_loc = loc->desc.ops->location_create(loc,&params)) == NULL;
			}
			rddma_location_put(loc);
		}
	}
	else if (params.ops) {
		ret = (new_loc = params.ops->location_create(NULL,&params)) == NULL;
	}
fail:
	if (result) {
		if (ret)
			ret = snprintf(result,size,"location_create://%s#%llx:%x?result(%d),reply(%s)\n",
				       params.name, params.offset, params.extent, ret,
				       rddma_get_option(&params,"request"));
		else
			ret = snprintf(result,size,"location_create://%s#%llx:%x?result(%d),reply(%s)\n",
				       new_loc->desc.name, new_loc->desc.offset,new_loc->desc.extent, ret,
				       rddma_get_option(&params,"request"));
	}

	rddma_clean_desc(&params);

	return ret;
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
static int location_delete(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_location *loc = NULL;
	struct rddma_desc_param params;

	RDDMA_DEBUG(MY_DEBUG, "%s %s\n",__FUNCTION__,desc);
	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -EINVAL;

	if ( params.location && *params.location) {
		if ( (loc = locate_rddma_location(NULL,&params) ) ) {
			if ( loc && loc->desc.ops && loc->desc.ops->location_delete ) {
				ret = 0;
				loc->desc.ops->location_delete(loc, &params);
			}
			rddma_location_put(loc);
		}
	}
	else if (params.ops) {
		ret = 0;
		params.ops->location_delete(NULL,&params);
	}
out:
	if (result)
		ret = snprintf(result,size,"location_delete://%s.%s?result(%d),reply(%s)\n", params.name, params.location,ret, rddma_get_option(&params,"request"));
	rddma_clean_desc(&params);

	return ret;
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
static int location_find(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_location *new_loc = NULL;
	struct rddma_location *loc = NULL;
	struct rddma_desc_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);
	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	RDDMA_DEBUG(MY_DEBUG,"%s %s,%s\n",__FUNCTION__,params.name,params.location);

	ret = -EINVAL;
	if (params.location && *params.location ) {
		if ( (loc = locate_rddma_location(NULL,&params)) ) {
			if (loc->desc.ops) {
				ret = ((new_loc = loc->desc.ops->location_find(loc,&params)) == NULL);
			}
			rddma_location_put(loc);
		}
	}
	else {
		if (params.ops)
			ret = (new_loc = params.ops->location_find(NULL,&params)) == NULL;
		else
			ret = (new_loc = find_rddma_name(NULL, &params)) == NULL;
	}
out:
	if (result) {
		if (ret)
			ret = snprintf(result,size,"location_find://%s#%llx:%x?result(%d),reply(%s)\n",
				       params.name,params.offset,params.extent,
				       ret, rddma_get_option(&params,"request"));
		else
			ret = snprintf(result,size,"location_find://%s#%llx:%x?result(%d),reply(%s)\n",
				       new_loc->desc.name,new_loc->desc.offset,new_loc->desc.extent,
				       ret, rddma_get_option(&params,"request"));
	}
	rddma_clean_desc(&params);

	return ret;
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
static int location_put(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_location *loc = NULL;
	struct rddma_desc_param params;

	RDDMA_DEBUG(MY_DEBUG, "%s %s\n",__FUNCTION__,desc);
	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -EINVAL;

	if (params.location && *params.location) {
		if ( (loc = locate_rddma_location(NULL,&params) ) ) {
			if ( loc && loc->desc.ops && loc->desc.ops->location_put ) {
				ret = 0;
				loc->desc.ops->location_put(loc, &params);
			}
			rddma_location_put(loc);
		}
	}
out:
	if (result)
		ret = snprintf(result,size,"location_put://%s.%s?result(%d),reply(%s)\n",
			       params.name, params.location,
			       ret, rddma_get_option(&params,"request"));
	rddma_clean_desc(&params);

	return ret;
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
static int smb_create(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_smb *smb = NULL;
	struct rddma_location *loc;
	struct rddma_desc_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	if (params.offset > (PAGE_SIZE - 32)) {
		ret = -EINVAL;
		goto out;
	}

	ret = -ENODEV;

	if ( (loc = locate_rddma_location(NULL,&params) ) ) {
		ret = -EINVAL;
		if (loc->desc.ops && loc->desc.ops->smb_create)
			ret = ((smb = loc->desc.ops->smb_create(loc, &params)) == NULL);
	}

	rddma_location_put(loc);

out:		
	if (result) {
		if (smb)
			ret = snprintf(result,size,"smb_create://%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       smb->desc.name, smb->desc.location,smb->desc.offset, smb->desc.extent,
				       ret, rddma_get_option(&params,"request"));
		else 
			ret = snprintf(result,size,"smb_create://%s.%s?result(%d),reply(%s)\n",
				       params.name, params.location,
				       ret, rddma_get_option(&params,"request"));
	}

	rddma_clean_desc(&params);

	return ret;
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
static int smb_delete(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_location *loc;
	struct rddma_desc_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (loc = locate_rddma_location(NULL,&params) ) ) {
		ret = -EINVAL;
		if ( loc->desc.ops && loc->desc.ops->smb_delete ) {
			ret = 0;
			loc->desc.ops->smb_delete(loc, &params);
		}
	}


	rddma_location_put(loc);

out:
	if (result) 
		ret = snprintf(result,size,"smb_delete://%s.%s?result(%d),reply(%s)\n", params.name, params.location,ret, rddma_get_option(&params,"request"));

	rddma_clean_desc(&params);

	return ret;
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
static int smb_find(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_smb *smb = NULL;
	struct rddma_location *loc;
	struct rddma_desc_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if (!params.location)  
		goto out;

	if ( (loc = locate_rddma_location(NULL,&params)) ) {
		ret = -EINVAL;
		
		if (loc->desc.ops && loc->desc.ops->smb_find)
			ret = ((smb = loc->desc.ops->smb_find(loc,&params)) == NULL);
	}

	rddma_location_put(loc);

out:
	if (result) {
		if (smb)
			ret = snprintf(result,size,"smb_find://%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       smb->desc.name, smb->desc.location,smb->desc.offset, smb->desc.extent, ret, rddma_get_option(&params,"request"));
		else
			ret = snprintf(result,size,"smb_find://%s.%s?result(%d),reply(%s)\n", params.name, params.location, ret, rddma_get_option(&params,"request"));
	}

	rddma_clean_desc(&params);

	return ret;
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
static int valid_extents(struct rddma_bind_param *x)
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
 * smb_mmap - Create mapping descriptor for mmap
 *
 * @desc:   Null-terminated string with parameters of operation
 * @result: Pointer to buffer to contain result string
 * @size:   Maximum size of result buffer
 *
 * This function services "smb_mmap" requests, whose job is to prepare
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
 * in an mmap call to the rddma device. See rddma_cdev.c::rddma_mmap()
 * for what happens next.
 *
 */
static int smb_mmap (const char* desc, char* result, int size)
{
	int ret = -ENOMEM;
	struct rddma_smb *smb = NULL;
	struct rddma_desc_param params;
	struct rddma_mmap *mmap = NULL;
	
	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	if (params.offset % PAGE_SIZE) {
		ret = -EINVAL;
		goto out;
	}

	ret = -ENODEV;

	if ( (smb = find_rddma_smb(&params)) ) {
		ret = -EINVAL;
		
		if (smb->desc.ops && smb->desc.ops->mmap_create)
			ret = ((mmap = smb->desc.ops->mmap_create (smb, &params)) == NULL);
	}

	rddma_smb_put(smb);

out:		
	if (result) {
		if (mmap) {
			/*
			* Note that because mmap identifiers are huge numbers, write
			* them as hex digits in the response.
			*/
			ret = snprintf(result,size,"%s?result(%d),reply(%s),mmap_offset(%lx)\n",
				       kobject_name (&mmap->kobj) ? : "<NULL>", ret, 
				       rddma_get_option(&params,"request"),
				       (unsigned long)mmap_to_ticket(mmap));
		}
		else {
			ret = snprintf(result,size,"smb_mmap://%s.%s?result(%d),reply(%s)\n", params.name, params.location,ret, rddma_get_option(&params,"request"));
		}
	}
	
	rddma_clean_desc(&params);

	return ret;
}

static int smb_unmmap (const char* desc, char* result, int size)
{
	int ret = -ENOMEM;
	struct rddma_smb *smb = NULL;
	struct rddma_desc_param params;
	
	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	if (params.offset % PAGE_SIZE) {
		ret = -EINVAL;
		goto out;
	}

	ret = -ENODEV;

	if ( (smb = find_rddma_smb(&params)) ) {
		ret = -EINVAL;
		
		if (smb->desc.ops && smb->desc.ops->mmap_delete) {
			ret = 0;
			smb->desc.ops->mmap_delete(smb,&params);
		}
	}

	rddma_smb_put(smb);

out:		
	if (result) {
		ret = snprintf(result,size,"smb_unmap://%s.%s?result(%d),reply(%s)\n", params.name, params.location, ret, rddma_get_option(&params,"request"));
	}
	
	rddma_clean_desc(&params);

	return ret;
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
static int xfer_create(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_xfer *xfer = NULL;
	struct rddma_location *location;
	struct rddma_desc_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (location = locate_rddma_location(NULL,&params) ) ) {
		ret = -EINVAL;
		
		if (location->desc.ops && location->desc.ops->xfer_create)
			ret = ((xfer = location->desc.ops->xfer_create(location, &params)) == NULL);
	}

	rddma_location_put(location);

out:		
	if (result)
		ret = snprintf(result,size,"xfer_create://%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.name, params.location,params.offset, params.extent,
			       ret,rddma_get_option(&params,"request"));

	rddma_clean_desc(&params);

	return ret;
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
static int xfer_delete(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_location *loc;
	struct rddma_desc_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (loc = locate_rddma_location(NULL,&params) ) ) {
		ret = -EINVAL;
		if ( loc->desc.ops && loc->desc.ops->xfer_delete ) {
			ret = 0;
			loc->desc.ops->xfer_delete(loc, &params);
		}
	}

	rddma_location_put(loc);

out:
	if (result) 
		ret = snprintf(result,size,"xfer_delete://%s.%s?result(%d),reply(%s)\n",
			       params.name, params.location,ret, rddma_get_option(&params,"request"));

	rddma_clean_desc(&params);

	return ret;
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
static int xfer_find(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_xfer *xfer = NULL;
	struct rddma_location *location;
	struct rddma_desc_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (location = locate_rddma_location(NULL,&params)) ) {
		ret = -EINVAL;
		if (location->desc.ops && location->desc.ops->xfer_find)
			ret = ((xfer = location->desc.ops->xfer_find(location,&params)) == NULL);
	}

	rddma_location_put(location);

out:
	if (result) {
		if (xfer)
			ret = snprintf(result,size,"xfer_find://%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.name, params.location,xfer->desc.offset, xfer->desc.extent,
				       ret,rddma_get_option(&params,"request"));
		else
			ret = snprintf(result,size,"xfer_find://%s.%s?result(%d),reply(%s)\n",
				       params.name, params.location,
				       ret,rddma_get_option(&params,"request"));
	}

	rddma_clean_desc(&params);

	return ret;
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
static int bind_create(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_xfer *xfer = NULL;
	struct rddma_bind *bind = NULL;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	/*
	* Parse <bind-spec> into <xfer-spec>, <dst-spec>, 
	* and <src-spec>.
	*
	*/
	if ( (ret = rddma_parse_bind(&params, desc)) )
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
	if ( (xfer = find_rddma_xfer(&params.xfer) ) ) {

		ret = -EINVAL;
		if (xfer->desc.ops && xfer->desc.ops->bind_create)
			ret = ((bind = xfer->desc.ops->bind_create(xfer, &params)) == NULL);
	}

	rddma_xfer_put(xfer);

out:		
	if (result) {
		if (bind)
			ret = snprintf(result,size,"bind_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       bind->desc.xfer.name, bind->desc.xfer.location, bind->desc.xfer.offset, bind->desc.xfer.extent,
				       bind->desc.dst.name,  bind->desc.dst.location,  bind->desc.dst.offset,  bind->desc.dst.extent,
				       bind->desc.src.name,  bind->desc.src.location,  bind->desc.src.offset,  bind->desc.src.extent,
				       ret,rddma_get_option(&params.src,"request"));
		else
			ret = snprintf(result,size,"bind_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name, params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name,params.dst.location, params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,rddma_get_option(&params.src,"request"));
	}

	rddma_clean_bind(&params);

	return ret;
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
static int bind_delete(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_xfer *xfer = NULL;
	struct rddma_desc_param params;
	
	RDDMA_DEBUG (MY_LIFE_DEBUG, "%s: \"%s\"\n", __FUNCTION__, desc);
	if ( (ret = rddma_parse_desc(&params, desc)) ) {
		RDDMA_DEBUG (MY_LIFE_DEBUG, "xx %s failed to parse bind_delete correctly\n", __FUNCTION__);
		goto out;
	}

	ret = -ENODEV;
	
	/*
	* Identify the xfer agent and instruct it to perform the bind_delete.
	*/
	if ( (xfer = find_rddma_xfer (&params) ) ) {
		ret = -EINVAL;
		/*
		* Check specified bind offset/extent values and substitute
		* or reject where necessary.
		*
		*/
		if (!params.soffset) params.offset = 0;
		if (!params.sextent) params.extent = xfer->desc.extent;
		if (!params.extent) {
			RDDMA_DEBUG (MY_LIFE_DEBUG, "xx %s failed: bind extent 0 not permitted!\n", __func__);
			goto out;
		}
		
		if ( xfer->desc.ops && xfer->desc.ops->bind_delete ) {
			ret = 0;
			xfer->desc.ops->bind_delete (xfer, &params);
		}
		else {
			RDDMA_DEBUG (MY_LIFE_DEBUG, "xx %s xfer %s has no bind_delete support\n", 
					__FUNCTION__, xfer->desc.name);
		}
		rddma_xfer_put (xfer);
	}
	else {
		RDDMA_DEBUG (MY_LIFE_DEBUG, "xx %s could not locate xfer %s\n", 
			     __FUNCTION__, params.name);
	}


out:
	if (result) 
		ret = snprintf(result,size,"bind_delete://%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.name, params.location,params.offset, params.extent,
			       ret,rddma_get_option(&params,"request"));

	rddma_clean_desc(&params);

	return ret;
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
static int bind_find(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_xfer *xfer = NULL;
	struct rddma_bind *bind = NULL;
	struct rddma_desc_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (xfer = find_rddma_xfer(&params)) ) {
		ret = -EINVAL;
		if (xfer->desc.ops && xfer->desc.ops->bind_find)
			ret = ((bind = xfer->desc.ops->bind_find(xfer,&params)) == NULL);
	}

	rddma_xfer_put(xfer);

out:
	if (result) {
		if (bind)
			ret = snprintf(result,size,"bind_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       bind->desc.xfer.name, bind->desc.xfer.location,bind->desc.xfer.offset, bind->desc.xfer.extent,
				       bind->desc.dst.name, bind->desc.dst.location,bind->desc.dst.offset, bind->desc.dst.extent,
				       bind->desc.src.name, bind->desc.src.location,bind->desc.src.offset, bind->desc.src.extent,
				       ret,rddma_get_option(&params,"request"));
		else
			ret = snprintf(result,size,"bind_find://%s.%s#%llx:%x/%s.%s=%s.%s#?result(%d),reply(%s)\n",
				       params.name, params.location, params.offset, params.extent,
				       params.name, params.location,
				       params.name, params.location,
				       ret,rddma_get_option(&params,"request"));
	}

	rddma_clean_desc(&params);

	return ret;
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
static int dst_create(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_dst *dst = NULL;
	struct rddma_bind *bind = NULL;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (bind = find_rddma_bind(&params.xfer) ) ) {
		ret = -EINVAL;
		if (bind->desc.xfer.ops && bind->desc.xfer.ops->dst_create)
			ret = ((dst = bind->desc.xfer.ops->dst_create(bind, &params)) == NULL);
	}

	RDDMA_KTRACE ("<*** %s bind put after dst_create opcall ***>\n", __func__);
	rddma_bind_put(bind);

out:		
	if (result) {
		if (dst)
			ret = snprintf(result,size,"dst_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_name(%s),result(%d),reply(%s)\n",
				       dst->desc.xfer.name,dst->desc.xfer.location,dst->desc.xfer.offset,dst->desc.xfer.extent,
				       dst->desc.dst.name,dst->desc.dst.location,dst->desc.dst.offset,dst->desc.dst.extent,
				       dst->desc.src.name,dst->desc.src.location,dst->desc.src.offset,dst->desc.src.extent,
				       rddma_get_option(&params.src,"event_name"),ret,rddma_get_option(&params.src,"request"));
		else
			ret = snprintf(result,size,"dst_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_name(%s),result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset,params.xfer.extent,
				       params.dst.name,params.dst.location,params.dst.offset,params.dst.extent,
				       params.src.name,params.src.location,params.src.offset,params.src.extent,
				       rddma_get_option(&params.src,"event_name"),ret,rddma_get_option(&params.src,"request"));
	}

	rddma_clean_bind(&params);

	return ret;
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
static int dst_delete(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_bind *bind = NULL;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;
	if ( (bind = find_rddma_bind(&params.xfer) ) ) {
		ret = -EINVAL;
		if ( bind->desc.xfer.ops && bind->desc.xfer.ops->dst_delete ) {
			ret = 0;
			bind->desc.xfer.ops->dst_delete(bind, &params);
		}
	}

	RDDMA_KTRACE ("<*** %s bind put after dst_delete opcall ***>\n", __func__);
	rddma_bind_put(bind);

out:
	if (result)
		ret = snprintf(result,size,"dst_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset,params.xfer.extent,
				       params.dst.name,params.dst.location,params.dst.offset,params.dst.extent,
				       params.src.name,params.src.location,params.src.offset,params.src.extent,
				       ret,rddma_get_option(&params.src,"request"));

	rddma_clean_bind(&params);

	return ret;
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
static int dst_find(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_dst *dst = NULL;
	struct rddma_bind *bind = NULL;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (bind = find_rddma_bind(&params.xfer)) ) {
		ret = -EINVAL;
		if (bind->desc.dst.ops && bind->desc.dst.ops->dst_find)
			ret = ((dst = bind->desc.dst.ops->dst_find(bind,&params)) == NULL);
	}

	RDDMA_KTRACE ("<*** %s bind put after dst_find opcall ***>\n", __func__);
	rddma_bind_put(bind);

out:
	if (result) {
		if (dst)
			ret = snprintf(result,size,"dst_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       dst->desc.xfer.name,dst->desc.xfer.location,dst->desc.xfer.offset, dst->desc.xfer.extent,
				       dst->desc.dst.name, dst->desc.dst.location,dst->desc.dst.offset, dst->desc.dst.extent,
				       dst->desc.src.name, dst->desc.src.location,dst->desc.src.offset, dst->desc.src.extent,
				       ret,rddma_get_option(&params.src,"request"));
		else
			ret = snprintf(result,size,"dst_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,rddma_get_option(&params.src,"request"));
	}

	rddma_clean_bind(&params);

	return ret;
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
static int src_create(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_src *src = NULL;
	struct rddma_dst *dst = NULL;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	dst = find_rddma_dst(&params);

	if (NULL == dst)
		goto out;

	ret = -EINVAL;

	if (dst->desc.xfer.ops && dst->desc.xfer.ops->src_create)
		ret = ((src = dst->desc.xfer.ops->src_create(dst, &params)) == NULL);


	rddma_dst_put(dst);

out:		
	if (result) {
		if (src)
			ret = snprintf(result,size,"src_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       src->desc.xfer.name, src->desc.xfer.location,src->desc.xfer.offset, src->desc.xfer.extent,
				       src->desc.dst.name, src->desc.dst.location,src->desc.dst.offset, src->desc.dst.extent,
				       src->desc.src.name, src->desc.src.location,src->desc.src.offset, src->desc.src.extent,
				       ret,rddma_get_option(&params.src,"request"));
		else
			ret = snprintf(result,size,"src_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,rddma_get_option(&params.src,"request"));
	}

	rddma_clean_bind(&params);

	return ret;
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
static int src_delete(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_dst *dst = NULL;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (dst = find_rddma_dst(&params) ) ) {
		ret = -EINVAL;
		if ( dst->desc.xfer.ops && dst->desc.xfer.ops->src_delete ) {
			ret = 0;
			dst->desc.xfer.ops->src_delete(dst, &params);
		}
		rddma_dst_put(dst);
	}


out:
	if (result)
		ret = snprintf(result,size,"src_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,rddma_get_option(&params.src,"request"));

	rddma_clean_bind(&params);

	return ret;
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
static int src_find(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_src *src = NULL;
	struct rddma_dst *dst = NULL;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (dst = find_rddma_dst(&params)) ) {
		ret = -EINVAL;
		if (dst->desc.dst.ops && dst->desc.dst.ops->src_find)
			ret = ((src = dst->desc.dst.ops->src_find(dst,&params)) == NULL);
	}

	rddma_dst_put(dst);

out:
	if (result) {
		if (src)
			ret = snprintf(result,size,"src_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       src->desc.xfer.name, src->desc.xfer.location,src->desc.xfer.offset, src->desc.xfer.extent,
				       src->desc.dst.name, src->desc.dst.location,src->desc.dst.offset, src->desc.dst.extent,
				       src->desc.src.name, src->desc.src.location,src->desc.src.offset, src->desc.src.extent,
				       ret,rddma_get_option(&params.src,"request"));
		else
			ret = snprintf(result,size,"src_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,rddma_get_option(&params.src,"request"));
	}

	rddma_clean_bind(&params);

	return ret;
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
static int srcs_create(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_srcs *srcs = NULL;
	struct rddma_dst *dst = NULL;
	struct rddma_bind *bind = NULL;
	int event_id = -1;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;
	
	if ( (dst = find_rddma_dst(&params) ) ) {
		ret = -EINVAL;

		if (dst->desc.src.ops && dst->desc.src.ops->srcs_create)
			ret = ((srcs = dst->desc.src.ops->srcs_create(dst, &params)) == NULL);

		bind = rddma_dst_parent(dst);
		event_id = bind->src_done_event->event_id;

		/*
		* TJA> Remove superfluous bind put.
		*
		* This put is unnecessary, and damaging - it causes an imbalance
		* in The Force that is noticed at delete time.
		*/
//		printk ("<*** %s bind put after srcs_create opcall ***>\n", __func__);
//		rddma_bind_put(bind);
	}

	rddma_dst_put(dst);

out:		
	if (result)
		ret = snprintf(result,size,"srcs_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_id(%d),result(%d),reply(%s)\n",
			       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
			       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
			       params.src.name, params.src.location,params.src.offset, params.src.extent,event_id,
			       ret,rddma_get_option(&params.src,"request"));
	rddma_clean_bind(&params);

	return ret;
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
static int srcs_delete(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_dst *dst = NULL;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;
	/*
	* Find the <dst> whose <srcs> are to be deleted.
	*
	* CAUTION: this call embedds further finds for xfer and
	* bind, and gives rise to refcount anomalies when <src> is
	* remote from <xfer>.
	*/
	if ( (dst = find_rddma_dst(&params) ) ) {
		ret = -EINVAL;
		if ( dst->desc.src.ops && dst->desc.src.ops->srcs_delete  ) {
			ret = 0;
			dst = dst->desc.src.ops->srcs_delete(dst, &params);
		}
		if (dst) rddma_dst_put(dst);	/* Counteract get from find, but only if dst still exists */
	}


out:
	if (result)
		ret = snprintf(result,size,"srcs_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,rddma_get_option(&params.src,"request"));

	rddma_clean_bind(&params);

	return ret;
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
static int srcs_find(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_srcs *srcs = NULL;
	struct rddma_dst *dst = NULL;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	if ( (dst = find_rddma_dst(&params)) ) {
		ret = -EINVAL;
		if (dst->desc.dst.ops && dst->desc.dst.ops->srcs_find)
			ret = ((srcs = dst->desc.dst.ops->srcs_find(dst,&params)) == NULL);
		rddma_dst_put(dst);
	}


out:
	if (result)
		ret = snprintf(result,size,"srcs_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
			       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
			       params.src.name, params.src.location,params.src.offset, params.src.extent,
			       ret,rddma_get_option(&params.src,"request"));
	rddma_clean_bind(&params);

	return ret;
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
static int dsts_create(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_dsts *dsts = NULL;
	struct rddma_bind *bind = NULL;
	struct rddma_bind_param params;
	int event_id = -1;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	/*
	* Parse the bind specification contained in the instruction into its
	* <xfer>, <dst>, and <src> components.
	*
	*/
	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;
	
	/*
	* Find the bind object in our tree. If the <xfer> agent is local,
	* it will already be there. If the <xfer> agent is remote, then
	* we will create a stub for it - provided it exists at the xfer site.
	*
	*/
	if ( (bind = find_rddma_bind(&params.xfer) ) ) {
		ret = -EINVAL;

		/*
		* With the bind and its components properly identified, 
		* invoke the dsts_create function at the <dst> site.
		*
		*/
		if (bind->desc.dst.ops && bind->desc.dst.ops->dsts_create)
			ret = ((dsts = bind->desc.dst.ops->dsts_create(bind, &params)) == NULL);

		event_id = bind->dst_done_event->event_id;
	}
	RDDMA_KTRACE ("<*** %s bind put after dsts_create opcall ***>\n", __func__);
	rddma_bind_put(bind);

out:		
	if (result)
		ret = snprintf(result,size,"dsts_create://%s.%s#%llx:%x/%s.%s#%llx:%x?event_id(%d)=%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
			       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
			       event_id,
			       params.src.name, params.src.location,params.src.offset, params.src.extent,
			       ret,rddma_get_option(&params.src,"request"));
	rddma_clean_bind(&params);

	return ret;
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
static int dsts_delete(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_bind *bind = NULL;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;
	/*
	* Find the target bind in the local object tree, then invoke
	* its <dst> agent dst_delete operation.
	*
	* NOTE: for sanity's sake, the bind "get" that is implicit in
	* this find_rddma_bind call is "put" by the API function. Otherwise
	* your head will explode if running a local dsts_delete.
	*/
	if ( (bind = find_rddma_bind(&params.xfer) ) ) {
		ret = -EINVAL;
		if ( bind->desc.dst.ops && bind->desc.dst.ops->dsts_delete ) {
			ret = 0;
			/*
			* Invoke the <dst> dsts_delete op to delete dsts.
			* That function returns a pointer to the parent bind
			* that we MUST take account of. If the return value
			* is NULL, it means that the bind, too, has been deleted
			* from the local tree.
			*/
			bind = bind->desc.dst.ops->dsts_delete(bind, &params);
		}
		
		if (bind) {
			RDDMA_KTRACE ("<*** %s bind put after dsts_delete opcall ***>\n", __func__);
			rddma_bind_put (bind);	/* Counteract get from "find", but only if bind still exists */
		}
	}

out:
	if (result)
		ret = snprintf(result,size,"dsts_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
				       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
				       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
				       params.src.name, params.src.location,params.src.offset, params.src.extent,
				       ret,rddma_get_option(&params.src,"request"));

	rddma_clean_bind(&params);

	return ret;
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
static int dsts_find(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_dsts *dsts = NULL;
	struct rddma_bind *bind = NULL;
	struct rddma_bind_param params;

	RDDMA_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,desc);

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	if ( (bind = find_rddma_bind(&params.xfer)) ) {
		ret = -EINVAL;
		if (bind->desc.dst.ops && bind->desc.dst.ops->dsts_find)
			ret = ((dsts = bind->desc.dst.ops->dsts_find(bind,&params)) == NULL);
	}

	RDDMA_KTRACE ("<*** %s bind put after dsts_find opcall ***>\n", __func__);
	rddma_bind_put(bind);

out:
	if (result)
		ret = snprintf(result,size,"dsts_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result(%d),reply(%s)\n",
			       params.xfer.name,params.xfer.location,params.xfer.offset, params.xfer.extent,
			       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
			       params.src.name, params.src.location,params.src.offset, params.src.extent,
			       ret,rddma_get_option(&params.src,"request"));
	rddma_clean_bind(&params);

	return ret;
}

static int event_start(const char *desc, char *result, int size)
{
	struct rddma_desc_param params;
	struct rddma_location *loc;
	int ret = -EINVAL;

	if ( (ret = rddma_parse_desc(&params,desc)) ) {
		goto out;
	}

	if ( params.location && *params.location ) {
		if ( (loc = locate_rddma_location(NULL,&params))) {
			if (loc && loc->desc.ops && loc->desc.ops->event_start) {
				ret = loc->desc.ops->event_start(loc,&params);
			}
			rddma_location_put(loc);
		}
	}
	else if (params.ops) {
		ret = params.ops->event_start(NULL,&params);
	}

out:
	if (result)
		ret = snprintf(result,size,"event_start://%s.%s?result(%d),reply(%s)\n",
			       params.name,params.location,
			       ret,rddma_get_option(&params,"request"));
	rddma_clean_desc(&params);

	return ret;
}

#define MAX_OP_LEN 15		/* length of location_create */
static struct ops {
	char *cmd;
	int (*f)(const char *, char *, int);
} ops [] = {
	{"location_create", location_create},
	{"location_delete", location_delete},
	{"location_find", location_find},
	{"location_put", location_put},
	{"smb_create", smb_create},
	{"smb_delete", smb_delete},
	{"smb_find", smb_find},
	{"smb_mmap", smb_mmap}, 
	{"smb_unmmap",smb_unmmap},
	{"xfer_create", xfer_create},
	{"xfer_delete", xfer_delete},
	{"xfer_find", xfer_find},
	{"bind_create", bind_create},
	{"bind_delete", bind_delete},
	{"bind_find", bind_find},
	{"src_create", src_create},
	{"src_delete", src_delete},
	{"src_find", src_find},
	{"dst_create", dst_create},
	{"dst_delete", dst_delete},
	{"dst_find", dst_find},
	{"srcs_create", srcs_create},
	{"srcs_delete", srcs_delete},
	{"srcs_find", srcs_find},
	{"dsts_create", dsts_create},
	{"dsts_delete", dsts_delete},
	{"dsts_find", dsts_find},
	{"event_start",event_start},
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

int do_operation(const char *cmd, char *result, int size)
{
	static int nested = 0;
	int ret = -EINVAL;
	char *sp1;
	char test[MAX_OP_LEN + 1];
	int toklen;
	int found = 0;
	int this_nested = ++nested;

	RDDMA_DEBUG (MY_DEBUG,"#### %s (%d) entered with %s, result=%p, size=%d\n",__FUNCTION__,this_nested,cmd,result, size);

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
		ret = snprintf(result,size,"%s%cresult(10101),reply(%x)\n" , cmd, request ? ',' : '?',reply);
	}

	nested--;
	RDDMA_DEBUG (MY_DEBUG, "#### [ done_operation (%d) \"%s\" ]\n", this_nested, cmd);
	return ret;
}

