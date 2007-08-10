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

static char *netdev_name = "eth0"; /* "eth0" or "br0" or "bond0" "rionet0" etc */
module_param(netdev_name, charp, 0644);

static unsigned short netdev_type = 0xfeed;
module_param(netdev_type, ushort, 0644);

struct fabric_address {
	unsigned char hw_address[ETH_ALEN];
	unsigned long idx;	/* ip address hint hint nod nod */
	struct net_device *ndev;
	struct list_head list;
	struct rddma_fabric_address rfa;
	struct kobject kobj;
};

static inline struct fabric_address *to_fabric_address(struct rddma_fabric_address *rfa)
{
	return rfa ? container_of(rfa,struct fabric_address,rfa) : NULL;
}

/* static struct fabric_address *source_address; */
static struct fabric_address *address_table[256];
static struct rddma_address_ops fabric_net_ops;
static struct kobj_type fabric_address_type;

static struct fabric_address *new_fabric_address(unsigned long idx, char *hwaddr, struct net_device *ndev)
{
	struct fabric_address *new = kzalloc(sizeof(struct fabric_address),GFP_KERNEL);

	INIT_LIST_HEAD(&new->list);

	new->kobj.ktype = &fabric_address_type;
	kobject_init(&new->kobj);

	new->idx = idx;
	new->ndev = ndev;

	if (hwaddr)
		strncpy(new->hw_address,hwaddr,ETH_ALEN);

	new->rfa.ops = &fabric_net_ops;
	new->rfa.owner = THIS_MODULE;

	return new;
}

static struct rddma_fabric_address *find_fabric_address(unsigned long idx, char *hwaddr, struct net_device *ndev)
{
	struct fabric_address *fp = address_table[idx & 15];
	struct fabric_address *new;
	if (fp) {
		if (fp->idx == idx) {

			if (hwaddr && strncmp(fp->hw_address,hwaddr,ETH_ALEN))
				memcpy(fp->hw_address,hwaddr,ETH_ALEN);

			if (ndev && !fp->ndev )
				fp->ndev = ndev;

			return &fp->rfa;
		}

		if (!list_empty(&fp->list)) {
			list_for_each_entry(new, &fp->list , list) {
				if (new->idx == idx) {
					if (hwaddr && strncmp(hwaddr,new->hw_address,ETH_ALEN) )
						memcpy(new->hw_address,hwaddr,ETH_ALEN);
					if ( ndev && !new->ndev ) 
						new->ndev = ndev;
					return &new->rfa;
				}
			}
		}

		new = new_fabric_address(idx,hwaddr,ndev);
		list_add_tail(&new->list,&fp->list);
		return &new->rfa;
	}

	address_table[idx & 15] = fp = new_fabric_address(idx,hwaddr,ndev);
	return &fp->rfa;
}

static void remove_fabric_address(struct fabric_address *addr)
{
	struct fabric_address *hp = address_table[addr->idx & 15];
	struct fabric_address *np, *fp;
	if (hp) {
		list_for_each_entry_safe(fp,np,&hp->list,list) {
			if (fp == addr) {
				list_del_init(&fp->list);
				if (fp == hp && hp != np)
					hp = np;
				break;
			}
		}
	}
}

static void fabric_address_release(struct kobject *kobj)
{
	struct fabric_address *old = kobj ? container_of(kobj, struct fabric_address, kobj) : NULL;
	if (old) {
		remove_fabric_address(old);
		kfree(old);
	}
}

static struct kobj_type fabric_address_type = {
	.release = fabric_address_release,
};

/* 
 * Frame format dstmac,srcmac,rddmatype,srclocidx,string
 *                6       6       2        4        n
 */

static int rddma_rx_packet(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
{
	struct rddma_fabric_address *rcvaddr = NULL;
	struct ethhdr *mac = eth_hdr(skb);
	unsigned long srcidx;

	memcpy(&srcidx,skb->data,4);
	be32_to_cpus((__u32 *)&srcidx);
	skb_pull(skb,4);

	rcvaddr = find_fabric_address(srcidx,mac->h_source,dev);

	return rddma_fabric_receive(rcvaddr,skb);
}

static int fabric_transmit(struct rddma_fabric_address *addr, struct sk_buff *skb)
{
	struct ethhdr *mac;

	int ret = NET_XMIT_DROP;
	struct fabric_address *faddr = to_fabric_address(addr);

	if (faddr->ndev) {
		mac  = (struct ethhdr *)skb_push(skb,ETH_HLEN+4);

		if (faddr->hw_address)
			memcpy(mac->h_dest,faddr->hw_address,ETH_ALEN);
		else
			memcpy(mac->h_dest,faddr->ndev->broadcast,ETH_ALEN);

		memcpy(mac->h_source, faddr->ndev->dev_addr,ETH_ALEN);

		mac->h_proto = htons(netdev_type);

/* 		memcpy(skb->data+ETH_HLEN,source_address->idx,4); */
		skb->dev = faddr->ndev;
		return dev_queue_xmit(skb);
	}

	dev_kfree_skb(skb);
	return ret;
}

struct packet_type rddma_packets = {
	.func = rddma_rx_packet,
};

static struct rddma_fabric_address *fabric_get(struct rddma_fabric_address *addr)
{
	return (addr ? kobject_get(&to_fabric_address(addr)->kobj) : NULL) ? addr : NULL; /* This is bound to be wrong... */
}

static void fabric_put(struct rddma_fabric_address *addr)
{
	struct fabric_address *fna;
	if ( (fna = to_fabric_address(addr)) )
		kobject_put(&fna->kobj);
}

static int fabric_register(struct rddma_location *loc)
{
	struct fabric_address *fna = to_fabric_address(loc->address);
	struct rddma_fabric_address *old = loc->address;
	char *ndev_name;
	struct net_device *ndev = NULL;

	if ( (ndev_name = rddma_get_option(&loc->desc,"netdev=")) )
		ndev = dev_get_by_name(ndev_name);
	else
		ndev = fna->ndev;

	loc->address = &new_fabric_address(loc->desc.offset,0,ndev)->rfa;
	fabric_put(old);
	return 0;
}

static void fabric_unregister(struct rddma_location *loc)
{
	fabric_put(loc->address);
}

static struct rddma_address_ops fabric_net_ops = {
	.transmit = fabric_transmit,
	.register_location = fabric_register,
	.unregister_location = fabric_unregister,
	.put = fabric_put,
	.get = fabric_get,
};

static int __init fabric_net_init(void)
{
	struct rddma_fabric_address *rfa;
	struct fabric_address *fna;
	if ( (fna = new_fabric_address(0,0,0)) ) {
		rfa = &fna->rfa;
		snprintf(rfa->name, RDDMA_MAX_FABRIC_NAME_LEN, "%s", "rddma_fabric_net");
		if (netdev_name)
			if ( (fna->ndev = dev_get_by_name(netdev_name)) ) {
				rddma_packets.dev = fna->ndev;
				dev_add_pack(&rddma_packets);
				return rddma_fabric_register(rfa);
			}
		return -ENODEV;
	}
	return -ENOMEM;
}

static void __exit fabric_net_close(void)
{
	dev_remove_pack(&rddma_packets);
	rddma_fabric_unregister("rddma_fabric_net");
}

module_init(fabric_net_init);
module_exit(fabric_net_close);

