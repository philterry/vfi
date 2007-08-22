/**************************************************************************
* Title     : rddma_mmap.h
* Author    : Trevor Anderson
* Copyright : (c) Micro Memory, 2007 - All Rights Reserved
*--------------------------------------------------------------------------
* Description : Definitions for mmap ticketing subsystem
*--------------------------------------------------------------------------
* 
* 
* This program is free software; you can redistribute  it and/or modify it
* under  the terms of  the GNU General  Public License as published by the
* Free Software Foundation;  either version 2 of the  License, or (at your
* option) any later version.
* 
* fcanzac
**************************************************************************/
#ifndef _RDDMA_MMAP_H_
#define _RDDMA_MMAP_H_
#include <asm/semaphore.h>

#define RDDMA_MMAP_TICKETS	16

struct rddma_mmap_ticket {
	unsigned long		t_id;		/* Ticket number, for verification */
	struct page		**pg_tbl;	/* Page table pointer to start of block */
	unsigned long		n_pg;		/* Number of pages in block */
};


struct rddma_mmap_book {
	struct semaphore 	 sem;		/* Mutex governing counters */
	unsigned long		 issued;	/* Number of tickets ever issued */
	unsigned long		 stamped;	/* Number of tickets serviced */
	struct rddma_mmap_ticket ticket[RDDMA_MMAP_TICKETS];
};


void rddma_mmap_tickets_init (void);
unsigned long rddma_mmap_create (struct page **pg_tbl, unsigned long n_pg, 
				 unsigned long offset, unsigned long extent);

struct rddma_mmap_ticket* rddma_mmap_find_ticket (unsigned long tid);
void rddma_mmap_stamp_ticket (unsigned long tid);












#endif

