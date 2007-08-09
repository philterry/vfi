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
#include <linux/module.h>
#include <linux/if_ether.h>

struct fabric_net_addr {
	struct rddma_location *locs[256];
	struct netdev *ndev;
	struct packet_type pt;
};

static int rddma_rx_packet(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
{
/* 	struct rddma_location *loc = NULL; */
/* 	struct ethhdr *mac = eth_hdr(skb); */
/* 	if ( (loc = fabric_net[mac->h_dest[5]])) */
/* 		return rddma_fabric_receive(loc,skb); */
	dev_kfree_skb(skb);
	return NET_RX_DROP;
}

static int fabric_transmit(struct rddma_fabric_address *addr, struct sk_buff *skb)
{
	/* Build the ethernet header on the front of the skb and then hard_start_xmit on the device */
	return 0;
}

static int fabric_register(struct rddma_location *loc)
{
	char *netdev_name;
	struct net_device *ndev;
/* 	struct fabric_net_addr *fna = (struct fabric_net_addr *)loc->address->address; */
	netdev_name = rddma_get_option(&loc->desc,"net_dev");
	ndev = dev_get_by_name(netdev_name);
/* 	dev_add_pack( &rddma_packets ); */
	return 0;
}

static void fabric_unregister(struct rddma_location *loc)
{
/* 	dev_remove_pack( &rddma_packets ); */
}

static struct rddma_fabric_address *fabric_get(struct rddma_fabric_address *addr)
{
	return container_of(kobject_get(&addr->kobj), struct rddma_fabric_address, kobj);
}

static void fabric_put(struct rddma_fabric_address *addr)
{
	kobject_put(&addr->kobj);
}

static struct rddma_address_ops rddma_fabric_net_ops = {
	.transmit = fabric_transmit,
	.register_location = fabric_register,
	.unregister_location = fabric_unregister,
	.get = fabric_get,
	.put = fabric_put,
};

static struct rddma_fabric_address net_fabric_address = {
	.owner = THIS_MODULE,
	.ops = &rddma_fabric_net_ops,
};

static int __init fabric_net_init(void)
{
	return rddma_fabric_register(&net_fabric_address);
}

static void __exit fabric_net_close(void)
{
	rddma_fabric_unregister(&net_fabric_address);
}

static char *netdev_name = NULL; /* "eth0" or "br0" or "bond0" etc */

module_init(fabric_net_init);
module_exit(fabric_net_close);

module_param(netdev_name, charp, 0644);
