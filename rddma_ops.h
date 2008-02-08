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

#ifndef RDDMA_OPS_H
#define RDDMA_OPS_H

#include <linux/rddma_parse.h>
#include <linux/rddma_location.h>
#include <linux/rddma_bind.h>

struct rddma_location;

struct rddma_ops {
	struct rddma_location * (*location_create)(struct rddma_location *, struct rddma_desc_param *);
	void                    (*location_delete)(struct rddma_location *, struct rddma_desc_param *);
	struct rddma_location *   (*location_find)(struct rddma_location *, struct rddma_desc_param *);
	void                       (*location_put)(struct rddma_location *, struct rddma_desc_param *);


	struct rddma_smb *           (*smb_create)(struct rddma_location *, struct rddma_desc_param *);
	void                         (*smb_delete)(struct rddma_location *, struct rddma_desc_param *);
	struct rddma_smb *             (*smb_find)(struct rddma_location *, struct rddma_desc_param *);

	struct rddma_mmap *         (*mmap_create)(struct rddma_smb *,      struct rddma_desc_param *);
	void                        (*mmap_delete)(struct rddma_smb *,      struct rddma_desc_param *);
	struct rddma_mmap *           (*mmap_find)(struct rddma_smb *,      struct rddma_desc_param *);

	struct rddma_xfer *         (*xfer_create)(struct rddma_location *, struct rddma_desc_param *);
	void                        (*xfer_delete)(struct rddma_location *, struct rddma_desc_param *);
	struct rddma_xfer *           (*xfer_find)(struct rddma_location *, struct rddma_desc_param *);

	struct rddma_bind *         (*bind_create)(struct rddma_xfer *,     struct rddma_bind_param *);
	void                        (*bind_delete)(struct rddma_xfer *,     struct rddma_desc_param *);
	struct rddma_bind *           (*bind_find)(struct rddma_xfer *,     struct rddma_desc_param *);
	
	struct rddma_dsts *         (*dsts_create)(struct rddma_bind *,     struct rddma_bind_param *);
	struct rddma_bind *         (*dsts_delete)(struct rddma_bind *,     struct rddma_bind_param *);
	struct rddma_dsts *           (*dsts_find)(struct rddma_bind *,     struct rddma_bind_param *);

	struct rddma_dst *           (*dst_create)(struct rddma_bind *,     struct rddma_bind_param *);
	void                         (*dst_delete)(struct rddma_bind *,     struct rddma_bind_param *);
	struct rddma_dst *             (*dst_find)(struct rddma_bind *,     struct rddma_bind_param *);

	struct rddma_srcs *         (*srcs_create)(struct rddma_dst *,      struct rddma_bind_param *);
	void                        (*srcs_delete)(struct rddma_dst *,      struct rddma_bind_param *);
	struct rddma_srcs *           (*srcs_find)(struct rddma_dst *,      struct rddma_bind_param *);

	struct rddma_src *           (*src_create)(struct rddma_dst *,      struct rddma_bind_param *);
	void                         (*src_delete)(struct rddma_dst *,      struct rddma_bind_param *);
	struct rddma_src *             (*src_find)(struct rddma_dst *,      struct rddma_bind_param *);

	void                           (*src_done)(struct rddma_bind *);
	void                           (*dst_done)(struct rddma_bind *);
	void                               (*done)(struct rddma_event *);
	void                          (*src_ready)(struct rddma_bind *);
	void                          (*dst_ready)(struct rddma_bind *);

	int                          (*dst_events)(struct rddma_bind *,     struct rddma_bind_param *);
	int                          (*src_events)(struct rddma_dst *,      struct rddma_bind_param *);
	int                         (*event_start)(struct rddma_location *, struct rddma_desc_param *);
};


extern struct rddma_ops rddma_local_ops;
extern struct rddma_ops rddma_fabric_ops;

extern int do_operation (const char *, char *, int);

#endif	/* RDDMA_OPS_H */
