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

#include <linux/rddma_subsys.h>
struct rddma_location;

#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
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
	int (*register_location)  (struct rddma_location *);
	void(*unregister_location)(struct rddma_location *);
	struct rddma_fabric_address *(*get)(struct rddma_fabric_address *);
	void (*put)(struct rddma_fabric_address *);
};

struct rddma_fabric_address {
	struct module *owner;
	struct rddma_address_ops *ops;
	char name[RDDMA_MAX_FABRIC_NAME_LEN+1];
};

/*
 * The most common form of interaction is the rpc, to send a request
 * to a location and return with the result of that request as an
 * skb. The invocation should be blocking with a timeout.
*/
extern struct sk_buff *rddma_fabric_call(struct rddma_location *, int, char *, ...) __attribute__((format(printf, 3,4)));

/*
 * Next a straightforward transmit an skb
 */

extern int __must_check rddma_fabric_tx(struct rddma_fabric_address *, struct sk_buff *);

/*
 * Next we need to be able to receive via any of the above interfaces
 * requests and responses and direct them accordingly. For this common
 * handling we require a common interface and function to handle them
 * which all of the underlying interfaces should manufacture by calling:
 */

extern int rddma_fabric_receive (struct rddma_fabric_address *,struct sk_buff * );

/*
 * Finally, register and unregister take care of linking everything up
 * with the underlying fabric interfaces based upon rddma_fabric_address
 * The rddma uses named interfaces when creating locations.
 */
extern int rddma_fabric_register(struct rddma_fabric_address *);
extern void rddma_fabric_unregister(const char *);

extern struct rddma_fabric_address *rddma_fabric_find(const char *);
extern struct rddma_fabric_address *rddma_fabric_get(struct rddma_fabric_address *);
extern void rddma_fabric_put(struct rddma_fabric_address *);
#endif	/* RDDMA_FABRIC_H */
