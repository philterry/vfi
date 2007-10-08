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

#include <linux/rddma_parse.h>
#include <linux/rddma_location.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_smb.h>
#include <linux/rddma_src.h>
#include <linux/rddma_dst.h>
#include <linux/rddma_xfer.h>
#include <linux/rddma_bind.h>
#include <linux/rddma_mmap.h>

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

	if ( params.location && *params.location && *params.name  ) {
		if ( (loc = find_rddma_location(&params))) {
			if (loc && loc->desc.ops && loc->desc.ops->location_create) 
				ret = (new_loc = loc->desc.ops->location_create(loc,&params)) == NULL;
			rddma_location_put(loc);
		}
	}
	else {
		if (params.ops)
			ret = (new_loc = params.ops->location_create(NULL,&params)) == NULL;
	}
fail:
	if (result)
		ret = snprintf(result,size,"%s?result=%d,reply=%s\n", params.name, ret, rddma_get_option(&params,"request"));
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

	if ( (loc = find_rddma_name(&params) ) ) {
		if ( loc && loc->desc.ops && loc->desc.ops->location_delete ) {
			ret = 0;
			loc->desc.ops->location_delete(loc, &params);
		}
		rddma_location_put(loc);
	}
out:
	if (result)
		ret = snprintf(result,size,"%s?result=%d,reply=%s\n", params.name, ret, rddma_get_option(&params,"request"));
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

	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);
	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -EINVAL;
	
	if (params.location && *params.location && *params.name) {
		if ( (loc = find_rddma_location(&params)) ) {
			if (loc->desc.ops && loc->desc.ops->location_find)
				ret = ((new_loc = loc->desc.ops->location_find(&params)) == NULL);
			rddma_location_put(loc);
		}
	}
	else {
		if (params.ops)
			ret = (new_loc = params.ops->location_find(&params)) == NULL;
	}
out:
	if (result) {
		if (ret)
			ret = snprintf(result,size,"%s.%s#%llx:%x?result=%d,reply=%s\n",params.name,params.location,params.offset,params.extent, ret, rddma_get_option(&params,"request"));
		else
			ret = snprintf(result,size,"%s.%s#%llx:%x?result=%d,reply=%s\n",new_loc->desc.name,new_loc->desc.location,new_loc->desc.offset,new_loc->desc.extent,
				       ret, rddma_get_option(&params,"request"));
	}
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

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	if (params.offset > (PAGE_SIZE - 32)) {
		ret = -EINVAL;
		goto out;
	}

	ret = -ENODEV;

	if ( (loc = find_rddma_location(&params) ) ) {
		ret = -EINVAL;
		if (loc->desc.ops && loc->desc.ops->smb_create)
			ret = ((smb = loc->desc.ops->smb_create(loc, &params)) == NULL);
	}

	rddma_location_put(loc);

out:		
	if (result) {
		if (smb)
			ret = snprintf(result,size,"%s.%s#%llx:%x?result=%d,reply=%s\n",
				       smb->desc.name, smb->desc.location,smb->desc.offset, smb->desc.extent, ret, rddma_get_option(&params,"request"));
		else 
			ret = snprintf(result,size,"%s.%s?result=%d,reply=%s\n", params.name, params.location,ret, rddma_get_option(&params,"request"));
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

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (loc = find_rddma_location(&params) ) ) {
		ret = -EINVAL;
		if ( loc->desc.ops && loc->desc.ops->smb_delete ) {
			ret = 0;
			loc->desc.ops->smb_delete(loc, &params);
		}
	}


	rddma_location_put(loc);

out:
	if (result) 
		ret = snprintf(result,size,"%s?result=%d,reply=%s\n", params.name, ret, rddma_get_option(&params,"request"));

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

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if (!params.location)  
		goto out;

	if ( (loc = find_rddma_location(&params)) ) {
		ret = -EINVAL;
		
		if (loc->desc.ops && loc->desc.ops->smb_find)
			ret = ((smb = loc->desc.ops->smb_find(loc,&params)) == NULL);
	}

	rddma_location_put(loc);

out:
	if (result) {
		if (smb)
			ret = snprintf(result,size,"%s.%s#%llx:%x?result=%d,reply=%s\n",
				       smb->desc.name, smb->desc.location,smb->desc.offset, smb->desc.extent, ret, rddma_get_option(&params,"request"));
		else
			ret = snprintf(result,size,"%s.%s?result=%d,reply=%s\n", params.name, params.location, ret, rddma_get_option(&params,"request"));
	}

	rddma_clean_desc(&params);

	return ret;
}

static int valid_extents(struct rddma_bind_param *x)
{
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
			ret = snprintf(result,size,"%s?result=%d,reply=%s,mmap_offset=%lx\n",
				       kobject_name (&mmap->kobj) ? : "<NULL>", ret, 
				       rddma_get_option(&params,"request"),
				       (unsigned long)mmap_to_ticket(mmap));
		}
		else {
			ret = snprintf(result,size,"%s?result=%d,reply=%s\n", params.name, ret, rddma_get_option(&params,"request"));
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
		ret = snprintf(result,size,"%s?result=%d,reply=%s\n", params.name, ret, rddma_get_option(&params,"request"));
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

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (location = find_rddma_location(&params) ) ) {
		ret = -EINVAL;
		
		if (location->desc.ops && location->desc.ops->xfer_create)
			ret = ((xfer = location->desc.ops->xfer_create(location, &params)) == NULL);
	}

	rddma_location_put(location);

out:		
	if (result) {
		if (xfer)
			ret = snprintf(result,size,"%s?result=%d,reply=%s\n",
				       xfer->desc.name, ret,rddma_get_option(&params,"request"));
		else
			ret = snprintf(result,size,"%s?result=%d,reply=%s\n", params.name, ret, rddma_get_option(&params,"request"));
	}

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

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (loc = find_rddma_location(&params) ) ) {
		ret = -EINVAL;
		if ( loc->desc.ops && loc->desc.ops->xfer_delete ) {
			ret = 0;
			loc->desc.ops->xfer_delete(loc, &params);
		}
	}

	rddma_location_put(loc);

out:
	if (result) 
		ret = snprintf(result,size,"%s?result=%d,reply=%s\n", params.name, ret, rddma_get_option(&params,"request"));

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

	if ( (ret = rddma_parse_desc(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (location = find_rddma_location(&params)) ) {
		ret = -EINVAL;
		if (location->desc.ops && location->desc.ops->xfer_find)
			ret = ((xfer = location->desc.ops->xfer_find(location,&params)) == NULL);
	}

	rddma_location_put(location);

out:
	if (result) {
		if (xfer)
			ret = snprintf(result,size,"%s?result=%d,reply=%s\n",
				       xfer->desc.name, ret,rddma_get_option(&params,"request"));
		else 
				ret = snprintf(result,size,"%s?result=%d,reply=%s\n", params.name, ret,rddma_get_option(&params,"request"));
	}

	rddma_clean_desc(&params);

	return ret;
}

/**
* xfer_start - initiate transfer
*
* @desc:   Null-terminated command string
* @result: Pointer to result string buffer
* @size:   Maximum size of result buffer
*
* This function executes an xfer_start request to initiate a transfer.
* It is a LOCAL operation that it is required to work on the local 
* instance of the named xfer. 
*
* Although the ultimate purpose of xfer_start is to initiate DMA
* transfers between source and destination buffers cited in the
* xfer bindings, that can only be accomplished indirectly. The
* immediate goal, here, is to notify the xfer agent (which may
* be remote) that all of the bind sources and destinations
* situated here, at the local site, are ready for a transfer to
* occur. The xfer agent will subsequently start actual transfers
* once all of its bind buffers are ready.
*
* We do this by traversing the bind tree of the local instance
* of the xfer, and for each bind invoking its src/dst notification
* routines. 
*
**/
static int xfer_start (const char*desc, char *result, int size)
{
	static int ret;
	struct rddma_xfer *xfer = NULL;
	struct rddma_location *location;
	struct rddma_desc_param params;

	if ((ret = rddma_parse_desc (&params, desc)))
		goto out;

	ret = -ENODEV;

	if ((xfer = find_rddma_xfer (&params))) {
		rddma_xfer_start (xfer);
		ret = 0;
	}
out:
	if (result) {
		if (xfer) 
			ret = snprintf(result,size,"%s?result=%d,reply=%s\n", 
				       xfer->desc.name, ret,rddma_get_option(&params,"request"));
		else 
			ret = snprintf(result,size,"%s?result=%d,reply=%s\n", params.name, ret,rddma_get_option(&params,"request"));
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
 */
static int bind_create(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_xfer *xfer = NULL;
	struct rddma_bind *bind = NULL;
	struct rddma_bind_param params;

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -EINVAL;

	if ( !valid_extents(&params) )
		goto out;

	ret = -ENODEV;

	if ( (xfer = find_rddma_xfer(&params.xfer) ) ) {
		if (!params.xfer.offset)
			params.xfer.offset = xfer->desc.extent;

		xfer->desc.extent += params.xfer.extent;

		ret = -EINVAL;
		if (xfer->desc.ops && xfer->desc.ops->bind_create)
			ret = ((bind = xfer->desc.ops->bind_create(xfer, &params)) == NULL);
	}

	rddma_xfer_put(xfer);

out:		
	if (result) {
		if (xfer && bind)
			ret = snprintf(result,size,"%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result=%d,reply=%s\n",
				       xfer->desc.name, xfer->desc.location,xfer->desc.offset, xfer->desc.extent,
				       bind->desc.dst.name,bind->desc.dst.location, bind->desc.dst.offset, bind->desc.dst.extent,
				       bind->desc.src.name, bind->desc.src.location,bind->desc.src.offset, bind->desc.src.extent,
				       ret,rddma_get_option(&params.xfer,"request"));
		else
			ret = snprintf(result,size,"%s.%s?result=%d,reply=%s\n", params.xfer.name,params.xfer.location, ret,rddma_get_option(&params.xfer,"request"));
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
 *    bind_delete://<xfer>#<xo>:<be>/<dst>#<do>:<be>=<src>#<so>:<be>
 *
 * Where the [xfer]/[dst]=[src] triplet specifies a bind uniquely:
 *
 *	<xfer> - name of transfer the binding belongs to
 *	<xo>   - Offset to the start of the bind within the transfer
 *	<be>   - Extent of the bind, in bytes
 *	<dst>  - Destination SMB that data will be copied to
 *	<do>   - Offset within Destination SMB where data will be copied to
 *	<src>  - Source SMB that data will be copied from
 *	<so>   - Offset within Source SMB where data will be copied from
 *
 * Deleting a bind involves four distinct agents: the requesting agent, who
 * receives the original command and who is running this function; the transfer 
 * agent that owns the transfer to which the bind belongs; and the source and 
 * destination agents who manage the SMBs that the bind ties together. 
 *
 * It is the TRANSFER AGENT who must co-ordinate the deletion. 
 *
 * The purpose of this function is to parse the original request into its 
 * xfer/dst/src components, and forward it to the transfer agent for execution.
 * Should the transfer agent be situated locally, command execution will commence.
 *
 */
static int bind_delete(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_xfer *xfer = NULL;
	struct rddma_bind_param params;
	
	RDDMA_DEBUG (MY_LIFE_DEBUG, "%s: \"%s\"\n", __FUNCTION__, desc);
	if ( (ret = rddma_parse_bind(&params, desc)) ) {
		RDDMA_DEBUG (MY_LIFE_DEBUG, "xx %s failed to parse bind correctly\n", __FUNCTION__);
		goto out;
	}

	ret = -ENODEV;
	
	/*
	* Identify the xfer agent and instruct it to perform the bind_delete.
	*/
	if ( (xfer = find_rddma_xfer (&params.xfer) ) ) {
		ret = -EINVAL;
		if ( xfer->desc.ops && xfer->desc.ops->bind_delete ) {
			ret = xfer->desc.ops->bind_delete (xfer, &params);
		}
		else {
			RDDMA_DEBUG (MY_LIFE_DEBUG, "xx %s xfer %s has no bind_delete support\n", 
					__FUNCTION__, xfer->desc.name);
		}
		rddma_xfer_put (xfer);
	}
	else {
		RDDMA_DEBUG (MY_LIFE_DEBUG, "xx %s could not locate xfer %s\n", 
			     __FUNCTION__, params.xfer.name);
	}


out:
	if (result) 
		ret = snprintf(result,size,"%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result=%d,reply=%s\n",
			       params.xfer.name, params.xfer.location,params.xfer.offset, params.xfer.extent,
			       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
			       params.src.name, params.src.location,params.src.offset, params.src.extent,
			       ret,rddma_get_option(&params.xfer,"request"));

	rddma_clean_bind(&params);

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
	struct rddma_bind_param params;

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (xfer = find_rddma_xfer(&params.xfer)) ) {
		ret = -EINVAL;
		if (xfer->desc.ops && xfer->desc.ops->bind_find)
			ret = ((bind = xfer->desc.ops->bind_find(xfer,&params)) == NULL);
	}

	rddma_xfer_put(xfer);

out:
	if (result) {
		ret = snprintf(result,size,"%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?result=%d,reply=%s\n",
			       params.xfer.name, params.xfer.location,params.xfer.offset, params.xfer.extent,
			       params.dst.name, params.dst.location,params.dst.offset, params.dst.extent,
			       params.src.name, params.src.location,params.src.offset, params.src.extent,
			       ret,rddma_get_option(&params.xfer,"request"));
	}

	rddma_clean_bind(&params);

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

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (bind = find_rddma_bind(&params) ) ) {
		ret = -EINVAL;
		if (bind->desc.dst.ops && bind->desc.dst.ops->dst_create)
			ret = ((dst = bind->desc.dst.ops->dst_create(bind, &params)) == NULL);
	}

	rddma_bind_put(bind);

out:		
	if (result) {
		if (bind)
			ret = snprintf(result,size,"%s.%s#%llx:%x=%s.%s#%llx:%x?result=%d,reply=%s\n",
				       bind->desc.dst.name,bind->desc.dst.location, bind->desc.dst.offset, bind->desc.dst.extent,
				       bind->desc.src.name, bind->desc.src.location,bind->desc.src.offset, bind->desc.src.extent,
				       ret,rddma_get_option(&params.xfer,"request"));
		else 
			ret = snprintf(result,size,"%s.%s?result=%d,reply=%s\n", params.xfer.name,params.xfer.location, ret,rddma_get_option(&params.xfer,"request"));
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
 */
static int dst_delete(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_bind *bind = NULL;
	struct rddma_bind_param params;

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (bind = find_rddma_bind(&params) ) ) {
		ret = -EINVAL;
		if ( bind->desc.dst.ops && bind->desc.dst.ops->dst_delete ) {
			ret = 0;
			bind->desc.dst.ops->dst_delete(bind, &params);
		}
	}

	rddma_bind_put(bind);

out:
	if (result)
		ret = snprintf(result,size,"%s?result=%d,reply=%s\n",params.xfer.name,ret,rddma_get_option(&params.xfer,"request"));

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

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (bind = find_rddma_bind(&params)) ) {
		ret = -EINVAL;
		if (bind->desc.dst.ops && bind->desc.dst.ops->dst_find)
			ret = ((dst = bind->desc.dst.ops->dst_find(bind,&params)) == NULL);
	}

	rddma_bind_put(bind);

out:
	if (result) {
		if (bind)
			ret = snprintf(result,size,"%s.%s#%llx:%x=%s.%s#%llx:%x?result=%d,reply=%s\n",
				       bind->desc.dst.name, bind->desc.dst.location,bind->desc.dst.offset, bind->desc.dst.extent,
				       bind->desc.src.name, bind->desc.src.location,bind->desc.src.offset, bind->desc.src.extent,
				       ret,rddma_get_option(&params.xfer,"request"));
		else
			ret = snprintf(result,size,"%s.%s?result=%d,reply=%s\n", params.xfer.name, params.xfer.location,ret,rddma_get_option(&params.xfer,"request"));
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
 */
static int src_create(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_src *src = NULL;
	struct rddma_dst *dst = NULL;
	struct rddma_bind_param params;

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;
	
	if ( (dst = find_rddma_dst(&params) ) ) {
		ret = -EINVAL;
		if (dst->desc.dst.ops && dst->desc.dst.ops->src_create)
			ret = ((src = dst->desc.dst.ops->src_create(dst, &params)) == NULL);
	}

	rddma_dst_put(dst);

out:		
	if (result) {
		if (src)
			ret = snprintf(result,size,"%s.%s#%llx:%x=%s.%s#%llx:%x?result=%d,reply=%s\n",
				       src->desc.dst.name, src->desc.dst.location,src->desc.dst.offset, src->desc.dst.extent,
				       src->desc.src.name, src->desc.src.location,src->desc.src.offset, src->desc.src.extent,
				       ret,rddma_get_option(&params.xfer,"request"));
		else
			ret = snprintf(result,size,"%s.%s?result=%d,reply=%s\n", params.xfer.name, params.xfer.location,ret,rddma_get_option(&params.xfer,"request"));
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

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;

	if ( (dst = find_rddma_dst(&params) ) ) {
		ret = -EINVAL;
		if ( dst->desc.dst.ops && dst->desc.dst.ops->src_delete ) {
			ret = 0;
			dst->desc.dst.ops->src_delete(dst, &params);
		}
	}

	rddma_dst_put(dst);

out:
	if (result)
		ret = snprintf(result,size,"%s?result=%d,reply=%s\n", params.xfer.name, ret,rddma_get_option(&params.xfer,"request"));

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
			ret = snprintf(result,size,"%s.%s#%llx:%x=%s.%s#%llx:%x?result=%d,reply=%s\n",
				       src->desc.dst.name, src->desc.dst.location,src->desc.dst.offset, src->desc.dst.extent,
				       src->desc.src.name, src->desc.src.location,src->desc.src.offset, src->desc.src.extent,
				       ret,rddma_get_option(&params.xfer,"request"));
		else
			ret = snprintf(result,size,"%s.%s?result=%d,reply=%s\n", params.xfer.name, params.xfer.location,ret,rddma_get_option(&params.xfer,"request"));
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
 */
static int srcs_create(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_srcs *srcs = NULL;
	struct rddma_dst *dst = NULL;
	struct rddma_bind_param params;

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;
	
	if ( (dst = find_rddma_dst(&params) ) ) {
		ret = -EINVAL;
		if (dst->desc.dst.ops && dst->desc.dst.ops->srcs_create)
			ret = ((srcs = dst->desc.dst.ops->srcs_create(dst, &params)) == NULL);
	}

	rddma_dst_put(dst);

out:		
	if (result)
		ret = snprintf(result,size,"%s?result=%d,reply=%s\n", params.xfer.name, ret,rddma_get_option(&params.xfer,"request"));

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
 */
static int srcs_delete(const char *desc, char *result, int size)
{
	int ret = -ENOMEM;
	struct rddma_dst *dst = NULL;
	struct rddma_bind_param params;

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	ret = -ENODEV;
	if ( (dst = find_rddma_dst(&params) ) ) {
		ret = -EINVAL;
		if ( dst->desc.dst.ops && dst->desc.dst.ops->srcs_delete ) {
			ret = 0;
			dst->desc.dst.ops->srcs_delete(dst, &params);
		}
	}

	rddma_dst_put(dst);

out:
	if (result)
		ret = snprintf(result,size,"%s?result=%d,reply=%s\n",params.xfer.name,ret,rddma_get_option(&params.xfer,"request"));

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

	if ( (ret = rddma_parse_bind(&params, desc)) )
		goto out;

	if ( (dst = find_rddma_dst(&params)) ) {
		ret = -EINVAL;
		if (dst->desc.dst.ops && dst->desc.dst.ops->srcs_find)
			ret = ((srcs = dst->desc.dst.ops->srcs_find(dst,&params)) == NULL);
	}

	rddma_dst_put(dst);

out:
	if (result)
		ret = snprintf(result,size,"%s?result=%d,reply=%s\n",params.xfer.name,ret,rddma_get_option(&params.xfer,"request"));

	rddma_clean_bind(&params);

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
	{"smb_create", smb_create},
	{"smb_delete", smb_delete},
	{"smb_find", smb_find},
	{"smb_mmap", smb_mmap}, 
	{"smb_unmmap",smb_unmmap},
	{"xfer_create", xfer_create},
	{"xfer_delete", xfer_delete},
	{"xfer_find", xfer_find},
	{"xfer_start", xfer_start}, 
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
	int ret = -EINVAL;
	char *sp1;
	char test[MAX_OP_LEN + 1];
	int toklen;
	int found = 0;

	RDDMA_DEBUG(MY_DEBUG,"#### %s entered with %s, result=%p, size=%d\n",__FUNCTION__,cmd,result, size);

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
		ret = snprintf(result,size,"Huh \"%s\"\n" ,cmd);
	}

	return ret;
}

