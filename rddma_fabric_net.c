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

#include <linux/rddma_fabric.h>
#include <linux/rddma_location.h>

#include <linux/netdevice.h>

static int rddma_rx_packet(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
{
	struct rddma_location *loc = NULL;
/* from the senders mac address we need to map to the senders location. */
	return rddma_fabric_receive(loc,skb);
}

static struct packet_type rddma_packets = {
	.type = __constant_htons(0xfeed),
	.func = rddma_rx_packet,
};


static int fabric_transmit(struct rddma_fabric_address *addr, struct sk_buff *skb)
{
	/* Build the ethernet header on the front of the skb and then hard_start_xmit on the device */
	return 0;
}

static int fabric_register(struct rddma_fabric_address *addr)
{
	rddma_packets.type = htons(addr->rcv_addr.ethertype);
	dev_add_pack( &rddma_packets );
	return 0;
}

static int fabric_unregister(struct rddma_fabric_address *addr)
{
	dev_remove_pack( &rddma_packets );
	return 0;
}

struct rddma_net_ops rddma_fabric_net_ops = {
	.fabric_transmit = fabric_transmit,
	.fabric_register = fabric_register,
	.fabric_unregister = fabric_unregister,
};
