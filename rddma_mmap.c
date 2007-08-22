/**************************************************************************
* Title     : rddma_mmap.c
* Author    : Trevor Anderson
* Copyright : (c) Micro Memory, 2007 - All Rights Reserved
*--------------------------------------------------------------------------
* Description : mmap ticketing subsystem for RDDMA
*--------------------------------------------------------------------------
* This file provides support for mmap ticketing within the RDDMA
* driver. This is a scheme in which the ability to mmap a named
* RDDMA construct - an SMB or a transfer, say - into user virtual
* memory can be offered despite the fact that RDDMA does not possess
* nor operate a unified page table. 
*
* 
* This program is free software; you can redistribute  it and/or modify it
* under  the terms of  the GNU General  Public License as published by the
* Free Software Foundation;  either version 2 of the  License, or (at your
* option) any later version.
* 
* fcanzac
**************************************************************************/
#include <linux/rddma.h>
#include <linux/rddma_mmap.h>
#define MY_DEBUG      RDDMA_DBG_OPS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG

static int ready = 0;
static struct rddma_mmap_book book;

/**
* rddma_mmap_tickets_init - Initialize mmap ticketing.
* 
* 
**/
void rddma_mmap_tickets_init (void)
{
	if (!ready) {
		memset (&book, 0, sizeof (struct rddma_mmap_book));
		sema_init (&book.sem, 1);
	}
	ready = 1;
}


/**
* rddma_mmap_create - validate mapping and create mmap ticket
*
* @pg_tbl:	Address of target page table
* @pg_len:	Number of pages in target area
* @offset:	Offset to map start, bytes
* @extent:	Extent of mapping, bytes
*
* This function validates a map request and creates a ticket to describe it.
*
* The @pg_tbl and @pg_len arguments identify the target memory area by its
* page table. They refer to the entire target page table, not some smaller
* part of it.
*
* The @offset and @extent arguments specify the region within the target area
* that is to be mapped, in terms of a byte offset and a byte extent.
*
* @offset and @extent are converted from bytes to pages, and are checked against
* the table dimensions. If the region fits, the function will acquire the next 
* free ticket in @book and fill-in the details. Both may be zero. The @extent
* argument is meaningless, really, and only included here for consistency: the
* plain fact is that the user specifies the true extent in the mmap call
* itself, and we cannot pre-empt that aspect of mmap here.
*
* The function will return a ticket identifier to be used in a forthcoming 
* mmap call, where it would substitute for "offset". 
*
* The function returns zero if it could not create a ticket.
*
* If it happens that the ticket book is full on entry, the function will INVALIDATE
* one of its current entries. Bad luck on whoever held it before - should have been
* quicker.
* 
**/
unsigned long rddma_mmap_create (struct page **pg_tbl, unsigned long n_pg, 
				 unsigned long offset, unsigned long extent)
{
	unsigned long tid;
	struct rddma_mmap_ticket* tkt;
	if (!ready) rddma_mmap_tickets_init ();
	RDDMA_DEBUG (MY_DEBUG, "rddma_mmap_create: %lu-bytes at offset %lu of %lu-page table %p\n", 
		extent, offset, n_pg, pg_tbl);
	offset >>= PAGE_SHIFT;
	extent = (extent >> PAGE_SHIFT) + ((extent & (PAGE_SIZE-1)) ? 1 : 0);
	if ((offset + extent) > n_pg) {
		RDDMA_DEBUG (MY_DEBUG, "xx Requested region exceeds page table.\n"); 
		return 0;
	}
	
	down (&book.sem);
	tid = ++book.issued;
	tkt = &book.ticket[tid % RDDMA_MMAP_TICKETS];
	if (tkt->t_id) {
		RDDMA_DEBUG (MY_DEBUG, "-- Revoking unused ticket#%lu!\n", tkt->t_id);
	}
	tkt->t_id = tid;
	tkt->pg_tbl = &pg_tbl[offset];
	tkt->n_pg = n_pg - offset;
	up (&book.sem);
	
	RDDMA_DEBUG (MY_DEBUG, "-- Assigned ticket#%lu to %lu pages at %p\n", tid, tkt->n_pg, tkt->pg_tbl);
	return (tid << PAGE_SHIFT);
}

/**
* rddma_mmap_find_ticket - find ticket with specified number
*
* @tid - ticket number to look up.
*
* This function tries to find a ticket with the specified identifier, 
* and returns a pointer to the ticket structure if successful. Or NULL
* if it can't find it.
*
* For the time-being we use a very simple ticketing scheme that uses
* a fixed-size ticket book. The low-order bits of a ticket identifier
* (id modulo book size) tell us which entry it ought to occupy. The
* stored identifier within that entry tells us whether it does, or 
* whether it has been cancelled and superceded by another.
*
**/
struct rddma_mmap_ticket* rddma_mmap_find_ticket (unsigned long tid)
{
	struct rddma_mmap_ticket* tkt;
	
	if (!ready) rddma_mmap_tickets_init ();
	RDDMA_DEBUG (MY_DEBUG, "-- Find mmap ticket %lu\n", tid);
	tkt = &book.ticket[tid % RDDMA_MMAP_TICKETS];
	return (tkt->t_id == tid) ? tkt : (struct rddma_mmap_ticket*)NULL;
}


/**
* rddma_mmap_stamp_ticket - stamp a ticket and free its slot
*
* @tid - ticket number
*
* This function stamps a ticket, renders it meaningless, and
* increments the stamped ticket counter that helps us determine
* how many live tickets are pending.
*
**/
void rddma_mmap_stamp_ticket (unsigned long tid)
{
	struct rddma_mmap_ticket* tkt;
	
	if (!ready) rddma_mmap_tickets_init ();
	down (&book.sem);
	tkt = &book.ticket[tid % RDDMA_MMAP_TICKETS];
	if (tkt->t_id == tid) {
		tkt->t_id = 0;
		book.stamped++;
	}
	up (&book.sem);
}














