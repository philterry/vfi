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

#ifndef RDDMA_FABRIC_H
#define RDDMA_FABRIC_H

#include <linux/vfi_subsys.h>
struct rddma_location;

#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
static inline unsigned char *skb_end_pointer(const struct sk_buff *skb)
{
	return skb->end;
}
static inline unsigned char *skb_tail_pointer(const struct sk_buff *skb)
{
	return skb->tail;
}

static inline void skb_reset_tail_pointer(struct sk_buff *skb)
{
	skb->tail = skb->data;
}

static inline void skb_set_tail_pointer(struct sk_buff *skb, const int offset)
{
	skb->tail = skb->data + offset;
}

static inline unsigned char *skb_transport_header(const struct sk_buff *skb)
{
	return skb->h.raw;
}

static inline void skb_reset_transport_header(struct sk_buff *skb)
{
	skb->h.raw = skb->data;
}

static inline void skb_set_transport_header(struct sk_buff *skb,
					    const int offset)
{
	skb->h.raw = skb->data + offset;
}

static inline unsigned char *skb_network_header(const struct sk_buff *skb)
{
	return skb->nh.raw;
}

static inline void skb_reset_network_header(struct sk_buff *skb)
{
	skb->nh.raw = skb->data;
}

static inline void skb_set_network_header(struct sk_buff *skb, const int offset)
{
	skb->nh.raw = skb->data + offset;
}

static inline unsigned char *skb_mac_header(const struct sk_buff *skb)
{
	return skb->mac.raw;
}

static inline int skb_mac_header_was_set(const struct sk_buff *skb)
{
	return skb->mac.raw != NULL;
}

static inline void skb_reset_mac_header(struct sk_buff *skb)
{
	skb->mac.raw = skb->data;
}

static inline void skb_set_mac_header(struct sk_buff *skb, const int offset)
{
	skb->mac.raw = skb->data + offset;
}
#endif /* Linux < 2.6.23 */

/* 
 * This module defines the interface to the bottom of the RDDMA driver
 * for its "fabric" interface. This interface can be implemented in a
 * number of ways, any or all of which may be active at the same time
 * for different locations. 
 *
 * First, it can execute directly on an underlying fabric. E.g., for
 * rapidio we could implement the rddma_fabric interface directly on
 * rapidio messages and doorbells using the rio driver
 * primitives. This is implemented in rddma_fabric_rio. For
 * alternative fabric xxx we would implement rddma_fabric_xxx.
 *
 * Second, we can execute on top of a netdevice layer using
 * dev_add_pack i.e., we can define an ethertype for use by rddma
 * protocols (possibly with its own version of arp as well). This is
 * implemented in rddma_fabric_net. With this approach the
 * multiplexing of the rddma_fabric service over multiple alternative
 * fabrics is automatically taken care of by the netdevice layer. A
 * netdevice for a rapidio fabric is already provided for by the linux
 * rionet driver which in turn runs on the message and doorbells of
 * the rio driver. Currently, rionet only uses a single mbox, 0, of
 * the two provided in hardware and both our rddma_fabric traffic and
 * ordinary TCP/IP traffic would share this mbox. We could therefore
 * implement a clone of rionet but using the second mbox. We could
 * call this rddma_net.
 *
 * Its not clear if there is any real performance/space advantage of
 * rddma_fabric_rio over a combined rddma_fabric_net/rddma_net
 * approach. If either of these were moved to mbox 0 and rionet were
 * moved to mbox 1 then we would get a prioritization of DMA traffic
 * over TCP/IP traffic. Clearly, this would be advantageous over a
 * rddma_fabric_net/rionet approach.
 *
 * Given the lack of hardware we may want to run the serivce over
 * rddma_fabric_net/ethernet for test purposes. This would immediately
 * and transparently run over rionet when hardware become
 * available. This could then be migrated to rddma_net as/when
 * performace/functionality issues become apparent.
 *
 * The role of rddma_fabric is therefore to abstract the above
 * differences in implementatin and provide a uniform interface to and
 * from the underlying implementation. The abstraction of an
 * addressable node on a network/fabric is the "location" type,
 * rddma_location. This should include an abstract address type.
 */

#define RDDMA_MAX_FABRICS 5
#define RDDMA_MAX_FABRIC_NAME_LEN 31

struct rddma_fabric_address;

struct rddma_address_ops {
	int (*transmit)  (struct rddma_fabric_address *, struct sk_buff *);
	int (*doorbell) (struct rddma_fabric_address *, int);
	int (*register_location)  (struct rddma_location *);
	void(*unregister_location)(struct rddma_location *);
	int (*register_doorbell)(void (*)(void *), void *);
	void(*unregister_doorbell)(int);
	struct rddma_fabric_address *(*get)(struct rddma_fabric_address *);
	void (*put)(struct rddma_fabric_address *);
};

struct rddma_fabric_address {
	struct module *owner;
	struct rddma_address_ops *ops;
	char name[RDDMA_MAX_FABRIC_NAME_LEN+1];
};

/*
 * First the abstraction wrappers for the downcalls
 */

extern int __must_check rddma_fabric_tx(struct rddma_fabric_address *, struct sk_buff *);
extern int __must_check rddma_fabric_doorbell(struct rddma_fabric_address *, int);
extern int rddma_address_register(struct rddma_location *);
extern void rddma_address_unregister(struct rddma_location *);
extern int rddma_doorbell_register(struct rddma_fabric_address *, void (*)(void *), void *);
extern void rddma_doorbell_unregister(struct rddma_fabric_address *, int);
extern void rddma_doorbell_send(struct rddma_fabric_address *, int);
/*
 * The most common form of interaction is the rpc, to send a request
 * to a location and return with the result of that request as an
 * skb. The invocation should be blocking with a timeout.
*/
extern int rddma_fabric_call(struct sk_buff **, struct rddma_location *, int, char *, ...) __attribute__((format(printf, 4,5)));

/*
 * Now for the upcalls
 */

/*
 * We need to be able to receive via any of the interfaces
 * request and response messages, doorbells, etc, and direct them
 * accordingly. For this common handling we require a common interface
 * and function to handle them which all of the underlying interfaces
 * should manufacture by calling:
 */

extern int rddma_fabric_receive (struct rddma_fabric_address *,struct sk_buff * );
extern void rddma_dbell_callback(void (*)(void *),void *);

/*
 * Finally, register and unregister take care of linking everything up
 * with the underlying fabric interfaces based upon rddma_fabric_address
 * The rddma uses named interfaces when creating locations.
 */
extern int rddma_fabric_register(struct rddma_fabric_address *);
extern void rddma_fabric_unregister(const char *);

/*
 * These are downcalls and a common interface for finding the fabric interface.
 */
extern struct rddma_fabric_address *rddma_fabric_get(struct rddma_fabric_address *);
extern void rddma_fabric_put(struct rddma_fabric_address *);
extern struct rddma_fabric_address *rddma_fabric_find(const char *);
#endif	/* RDDMA_FABRIC_H */
