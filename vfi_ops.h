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

#ifndef VFI_OPS_H
#define VFI_OPS_H

#include <linux/vfi_parse.h>
#include <linux/vfi_location.h>
#include <linux/vfi_bind.h>

struct vfi_location;
struct vfi_smb;
struct vfi_mmap;
struct vfi_srcs;
struct vfi_dsts;
struct vfi_dst;
struct vfi_src;
struct vfi_sync;

struct vfi_ops {
	int  (*location_create)(struct vfi_location **, struct vfi_location *, struct vfi_desc_param *);
	int  (*location_find)  (struct vfi_location **, struct vfi_location *, struct vfi_desc_param *);
	void (*location_delete)(struct vfi_location *,  struct vfi_desc_param *);
	void (*location_put)   (struct vfi_location *,  struct vfi_desc_param *);
	void (*location_lose)  (struct vfi_location *,  struct vfi_desc_param *);

	int  (*smb_create)(struct vfi_smb **,     struct vfi_location *, struct vfi_desc_param *);
	int  (*smb_find)  (struct vfi_smb **,     struct vfi_location *, struct vfi_desc_param *);
	void (*smb_delete)(struct vfi_smb *,      struct vfi_desc_param *);
	void (*smb_put)   (struct vfi_smb *,      struct vfi_desc_param *);
	void (*smb_lose)  (struct vfi_smb *,      struct vfi_desc_param *);

	int  (*xfer_create)(struct vfi_xfer **,    struct vfi_location *, struct vfi_desc_param *);
	int  (*xfer_find)  (struct vfi_xfer **,    struct vfi_location *, struct vfi_desc_param *);
	void (*xfer_delete)(struct vfi_xfer *,     struct vfi_desc_param *);
	void (*xfer_put)   (struct vfi_xfer *,     struct vfi_desc_param *);
	void (*xfer_lose)  (struct vfi_xfer *,     struct vfi_desc_param *);

	int  (*sync_create)(struct vfi_sync **,    struct vfi_location *, struct vfi_desc_param *);
	int  (*sync_find)  (struct vfi_sync **,    struct vfi_location *, struct vfi_desc_param *);
	void (*sync_delete)(struct vfi_sync *,     struct vfi_desc_param *);
	void (*sync_put)   (struct vfi_sync *,     struct vfi_desc_param *);
	int  (*sync_send)  (struct vfi_sync *,     struct vfi_desc_param *);
	int  (*sync_wait)  (struct vfi_sync *,     struct vfi_desc_param *);
	int  (*sync_lose)  (struct vfi_sync *,     struct vfi_desc_param *);

	int  (*mmap_create)(struct vfi_mmap **,  struct vfi_smb *,      struct vfi_desc_param *);
	int  (*mmap_find)  (struct vfi_mmap **,  struct vfi_smb *,      struct vfi_desc_param *);
	void (*mmap_delete)(struct vfi_mmap *,   struct vfi_desc_param *);
	void (*mmap_put)   (struct vfi_mmap *,   struct vfi_desc_param *);

	int  (*bind_create)(struct vfi_bind **,    struct vfi_xfer *,     struct vfi_bind_param *);
	int  (*bind_find)  (struct vfi_bind **,    struct vfi_xfer *,     struct vfi_desc_param *);
	void (*bind_delete)(struct vfi_xfer *,     struct vfi_desc_param *);
	void (*bind_lose)  (struct vfi_xfer *,     struct vfi_desc_param *);
	
	int  (*dsts_create)(struct vfi_dsts **,    struct vfi_bind *,     struct vfi_bind_param *);
	int  (*dsts_find)  (struct vfi_dsts **,    struct vfi_bind *,     struct vfi_bind_param *);
	void (*dsts_delete)(struct vfi_bind *,     struct vfi_bind_param *);
	void (*dsts_put)   (struct vfi_bind *,     struct vfi_bind_param *);

	int  (*dst_create)(struct vfi_dst **,      struct vfi_bind *,     struct vfi_bind_param *);
	int  (*dst_find)  (struct vfi_dst **,      struct vfi_bind *,     struct vfi_bind_param *);
	void (*dst_delete)(struct vfi_bind *,      struct vfi_bind_param *);
	void (*dst_put)   (struct vfi_bind *,      struct vfi_bind_param *);

	int  (*srcs_create)(struct vfi_srcs **,    struct vfi_dst *,      struct vfi_bind_param *);
	int  (*srcs_find)  (struct vfi_srcs **,    struct vfi_dst *,      struct vfi_bind_param *);
	void (*srcs_delete)(struct vfi_dst *,      struct vfi_bind_param *);
	void (*srcs_put)   (struct vfi_dst *,      struct vfi_bind_param *);

	int  (*src_create)(struct vfi_src **,      struct vfi_dst *,      struct vfi_bind_param *);
	int  (*src_find)  (struct vfi_src **,      struct vfi_dst *,      struct vfi_bind_param *);
	void (*src_delete)(struct vfi_dst *,       struct vfi_bind_param *);
	void (*src_put)   (struct vfi_dst *,       struct vfi_bind_param *);

	void                           (*src_done)(struct vfi_bind *);
	void                           (*dst_done)(struct vfi_bind *);
	void                               (*done)(struct vfi_event *);
	void                          (*src_ready)(struct vfi_bind *);
	void                          (*dst_ready)(struct vfi_bind *);

	int                          (*dst_events)(struct vfi_bind *,     struct vfi_bind_param *);
	int                          (*src_events)(struct vfi_dst *,      struct vfi_bind_param *);
	void		          (*dst_ev_delete)(struct vfi_bind *,     struct vfi_bind_param *);
	void		          (*src_ev_delete)(struct vfi_dst *,      struct vfi_bind_param *);
	int                         (*event_start)(struct vfi_location *, struct vfi_desc_param *);
	int                          (*event_find)(struct vfi_location *, struct vfi_desc_param *);
	int                         (*event_chain)(struct vfi_location *, struct vfi_desc_param *);
};


extern struct vfi_ops vfi_local_ops;
extern struct vfi_ops vfi_fabric_ops;

extern int do_operation (const char *, char *, int *);

#endif	/* VFI_OPS_H */
