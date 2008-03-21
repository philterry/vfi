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

#ifndef VFI_FABRIC_H
#define VFI_FABRIC_H

#include <linux/vfi_subsys.h>
struct vfi_location;

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
 * This module defines the interface to the bottom of the VFI driver
 * for its "fabric" interface. This interface can be implemented in a
 * number of ways, any or all of which may be active at the same time
 * for different locations. 
 *
 * First, it can execute directly on an underlying fabric. E.g., for
 * rapidio we could implement the vfi_fabric interface directly on
 * rapidio messages and doorbells using the rio driver
 * primitives. This is implemented in vfi_fabric_rio. For
 * alternative fabric xxx we would implement vfi_fabric_xxx.
 *
 * Second, we can execute on top of a netdevice layer using
 * dev_add_pack i.e., we can define an ethertype for use by vfi
 * protocols (possibly with its own version of arp as well). This is
 * implemented in vfi_fabric_net. With this approach the
 * multiplexing of the vfi_fabric service over multiple alternative
 * fabrics is automatically taken care of by the netdevice layer. A
 * netdevice for a rapidio fabric is already provided for by the linux
 * rionet driver which in turn runs on the message and doorbells of
 * the rio driver. Currently, rionet only uses a single mbox, 0, of
 * the two provided in hardware and both our vfi_fabric traffic and
 * ordinary TCP/IP traffic would share this mbox. We could therefore
 * implement a clone of rionet but using the second mbox. We could
 * call this vfi_net.
 *
 * Its not clear if there is any real performance/space advantage of
 * vfi_fabric_rio over a combined vfi_fabric_net/vfi_net
 * approach. If either of these were moved to mbox 0 and rionet were
 * moved to mbox 1 then we would get a prioritization of DMA traffic
 * over TCP/IP traffic. Clearly, this would be advantageous over a
 * vfi_fabric_net/rionet approach.
 *
 * Given the lack of hardware we may want to run the serivce over
 * vfi_fabric_net/ethernet for test purposes. This would immediately
 * and transparently run over rionet when hardware become
 * available. This could then be migrated to vfi_net as/when
 * performace/functionality issues become apparent.
 *
 * The role of vfi_fabric is therefore to abstract the above
 * differences in implementatin and provide a uniform interface to and
 * from the underlying implementation. The abstraction of an
 * addressable node on a network/fabric is the "location" type,
 * vfi_location. This should include an abstract address type.
 */

#define VFI_MAX_FABRICS 5
#define VFI_MAX_FABRIC_NAME_LEN 31

struct vfi_fabric_address;

struct vfi_address_ops {
	int (*transmit)  (struct vfi_fabric_address *, struct sk_buff *);
	int (*doorbell) (struct vfi_fabric_address *, int);
	int (*register_location)  (struct vfi_location *);
	void(*unregister_location)(struct vfi_location *);
	int (*register_doorbell)(void (*)(void *), void *);
	void(*unregister_doorbell)(int);
	struct vfi_fabric_address *(*get)(struct vfi_fabric_address *);
	void (*put)(struct vfi_fabric_address *);
};

struct vfi_fabric_address {
	struct module *owner;
	struct vfi_address_ops *ops;
	char name[VFI_MAX_FABRIC_NAME_LEN+1];
};

/*
 * First the abstraction wrappers for the downcalls
 */

extern int __must_check vfi_fabric_tx(struct vfi_fabric_address *, struct sk_buff *);
extern int __must_check vfi_fabric_doorbell(struct vfi_fabric_address *, int);
extern int vfi_address_register(struct vfi_location *);
extern void vfi_address_unregister(struct vfi_location *);
extern int vfi_doorbell_register(struct vfi_fabric_address *, void (*)(void *), void *);
extern void vfi_doorbell_unregister(struct vfi_fabric_address *, int);
extern void vfi_doorbell_send(struct vfi_fabric_address *, int);
/*
 * The most common form of interaction is the rpc, to send a request
 * to a location and return with the result of that request as an
 * skb. The invocation should be blocking with a timeout.
*/
extern int vfi_fabric_call(struct sk_buff **, struct vfi_location *, int, char *, ...) __attribute__((format(printf, 4,5)));

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

extern int vfi_fabric_receive (struct vfi_fabric_address *,struct sk_buff * );
extern void vfi_dbell_callback(void (*)(void *),void *);

/*
 * Finally, register and unregister take care of linking everything up
 * with the underlying fabric interfaces based upon vfi_fabric_address
 * The vfi uses named interfaces when creating locations.
 */
extern int vfi_fabric_register(struct vfi_fabric_address *);
extern void vfi_fabric_unregister(const char *);

/*
 * These are downcalls and a common interface for finding the fabric interface.
 */
extern struct vfi_fabric_address *vfi_fabric_get(struct vfi_fabric_address *);
extern void vfi_fabric_put(struct vfi_fabric_address *);
extern struct vfi_fabric_address *vfi_fabric_find(const char *);
#endif	/* VFI_FABRIC_H */
