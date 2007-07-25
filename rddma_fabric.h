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
struct rddma_fabric_address;

struct rddma_net_ops {
	int (*fabric_transmit)(struct rddma_fabric_address *, struct sk_buff *);
	int (*fabric_register)(struct rddma_fabric_address *);
	int (*fabric_unregister)(struct rddma_fabric_address *);
};

struct rddma_fabric_address {
	struct rddma_net_ops *ops;
	union {
		int rio_id;
		char eth_addr[6];
	} send_addr;
	union {
		int rio_id;
		char eth_addr[6];
		short ethertype;
	} rcv_addr;
};

/*
 * The most comomon form of interaction is the rpc, to send a request
 * to a location and return with the result of that request as an
 * skb. The invocation should be blocking with a timeout.
*/
extern struct sk_buff *rddma_fabric_call(struct rddma_location *, int, char *, ...) __attribute__((format(printf, 3,4)));

/*
 * Next we need to be able to receive via any of the above interfaces
 * requests and responses and direct them accordingly. For this common
 * handling we require a common interface and function to handle them
 * which all of the underlying interfaces should manufacture by calling:
 */

extern int rddma_fabric_receive (struct rddma_location *,struct sk_buff * );

/*
 * Finally, register and unregister take care of linking everything up
 * with the underlying fabric interfaces based upon rddma_fabric_address
 * which must therefore be set up in the location before calling these:
 */
extern int rddma_fabric_register(struct rddma_location *);
extern void rddma_fabric_unregister(struct rddma_location *);

#endif	/* RDDMA_FABRIC_H */
