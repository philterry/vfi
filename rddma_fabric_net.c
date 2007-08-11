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
module_param(netdev_name, charp, 0444);

static unsigned short netdev_type = 0xfeed;
module_param(netdev_type, ushort, 0444);

static unsigned long default_dest = 0;
module_param(default_dest,ulong,0444);

struct fabric_address {
	unsigned char hw_address[ETH_ALEN];
	unsigned long idx;	/* ip address hint hint nod nod */
	struct rddma_location *reg_loc;
#define UNKNOWN_IDX ~0UL
	struct net_device *ndev;
	struct list_head list;
	struct rddma_fabric_address rfa;
	struct kobject kobj;
};

static inline struct fabric_address *to_fabric_address(struct rddma_fabric_address *rfa)
{
	return rfa ? container_of(rfa,struct fabric_address,rfa) : NULL;
}

static void fabric_address_release(struct kobject *kobj)
{
	struct fabric_address *old = kobj ? container_of(kobj, struct fabric_address, kobj) : NULL;
	if (old) {
		kfree(old);
	}
}

static struct kobj_type fabric_address_type = {
	.release = fabric_address_release,
};

static inline struct fabric_address *_fabric_get(struct fabric_address *fna)
{
	return (fna ? kobject_get(&fna->kobj), fna : NULL) ;
}

static inline void _fabric_put(struct fabric_address *fna)
{
	if ( fna ) kobject_put(&fna->kobj);
}

static struct fabric_address *address_table[256];

static void update_fabric_address(struct fabric_address *fp, char *hwaddr, struct net_device *ndev)
{
	if (hwaddr && strncmp(fp->hw_address,hwaddr,ETH_ALEN))
		memcpy(fp->hw_address,hwaddr,ETH_ALEN);
	
	if (ndev && !fp->ndev )
		fp->ndev = ndev;
}

static struct rddma_address_ops fabric_net_ops;

static struct fabric_address *new_fabric_address(unsigned long idx, char *hwaddr, struct net_device *ndev)
{
	struct fabric_address *new = kzalloc(sizeof(struct fabric_address),GFP_KERNEL);

	INIT_LIST_HEAD(&new->list);

	new->kobj.ktype = &fabric_address_type;
	kobject_init(&new->kobj);

	new->idx = idx;

	update_fabric_address(new,hwaddr,ndev);

	new->rfa.ops = &fabric_net_ops;
	new->rfa.owner = THIS_MODULE;

	return new;
}

static struct fabric_address *find_fabric_address(unsigned long idx, char *hwaddr, struct net_device *ndev)
{
	struct fabric_address *fp = address_table[idx & 15];
	struct fabric_address *new;

	if ( idx == UNKNOWN_IDX)
		return new_fabric_address(idx,hwaddr,ndev);

	if (fp) {
		if (fp->idx == idx) {
			update_fabric_address(fp,hwaddr,ndev);
			_fabric_get(fp);
			return fp;
		}

		if (!list_empty(&fp->list)) {
			list_for_each_entry(new, &fp->list , list) {
				if (new->idx == idx) {
					update_fabric_address(new,hwaddr,ndev);
					_fabric_get(new);
					return new;
				}
			}
		}

		new = new_fabric_address(idx,hwaddr,ndev);
		list_add_tail(&new->list,&fp->list);
		return new;
	}

	address_table[idx & 15] = fp = new_fabric_address(idx,hwaddr,ndev);
	return fp;
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
				_fabric_put(fp);
				break;
			}
		}
	}
}

/* 
 * Frame format dstmac,srcmac,rddmatype,dstidx,srcidx,string
 *                6       6       2        4      4      n
 *
 * All frames have srcmac.
 * dstmac broadcast means source doesn't know you but may be talking to you
 * dstmac unicast and yours means source is talking to you.
 * dstmac unicast and not you means drop and ignore.
 * srcidx -1 means I don't know who I am (bootp ;-)
 * dstidx -1 means I don't know who you are (hello syscontrol, dns, bootp, etc, ;-)
 *
 */

struct rddmahdr {
	unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
	unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
	__be16		h_proto;		/* packet type ID field	*/
	__be32          h_dstidx;
	__be32          h_srcidx;
} __attribute__((packed));

static inline struct rddmahdr *rddma_hdr(struct sk_buff *skb)
{
	skb_pull(skb, (sizeof(struct rddmahdr) - sizeof(struct ethhdr)));
	return (struct rddmahdr *)eth_hdr(skb);
}

static int rddma_rx_packet(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
{
	struct rddma_fabric_address *rcvaddr = NULL;
	struct fabric_address *fna;
	struct rddmahdr *mac = rddma_hdr(skb);
	unsigned long srcidx, dstidx;

	memcpy(&dstidx, &mac->h_dstidx, 4);
	be32_to_cpus((__u32 *)&dstidx);

	memcpy(&srcidx, &mac->h_srcidx, 4);
	be32_to_cpus((__u32 *)&srcidx);

	if ( (dstidx == UNKNOWN_IDX) && (mac->h_dest[0] & 1) )
		/* If you don't know who and you don't know where then we can't help you. */
		goto out;

	fna = find_fabric_address(srcidx,mac->h_source,dev);
	
	rcvaddr = &fna->rfa;
	
	return rddma_fabric_receive(rcvaddr,skb);
out:
	kfree_skb(skb);
	return NET_RX_DROP;
}

struct packet_type rddma_packets = {
	.func = rddma_rx_packet,
};

static int fabric_transmit(struct rddma_fabric_address *addr, struct sk_buff *skb)
{
	struct rddmahdr *mac;
	unsigned long srcidx,dstidx;

	int ret = NET_XMIT_DROP;
	struct fabric_address *fna = to_fabric_address(addr);

	if (fna->ndev) {
		mac  = (struct rddmahdr *)skb_push(skb,sizeof(struct rddmahdr));

		if (fna->hw_address)
			memcpy(mac->h_dest,fna->hw_address,ETH_ALEN);
		else
			memcpy(mac->h_dest,fna->ndev->broadcast,ETH_ALEN);

		memcpy(mac->h_source, fna->ndev->dev_addr,ETH_ALEN);

		mac->h_proto = htons(netdev_type);

		if (fna->reg_loc) {
			dstidx = htonl(fna->reg_loc->desc.extent);
			srcidx = htonl(fna->reg_loc->desc.offset);
		}
		else {
			dstidx = htonl(default_dest);
			srcidx = htonl(UNKNOWN_IDX);
		}
		memcpy(&mac->h_srcidx, &srcidx, sizeof(mac->h_srcidx));
		memcpy(&mac->h_dstidx, &dstidx, sizeof(mac->h_dstidx));

		skb->dev = fna->ndev;
		_fabric_put(fna);
		return dev_queue_xmit(skb);
	}

	_fabric_put(fna);
	dev_kfree_skb(skb);
	return ret;
}

static struct rddma_fabric_address *fabric_get(struct rddma_fabric_address *rfa)
{
	return rfa ? _fabric_get(to_fabric_address(rfa)), rfa : NULL;
}

static void fabric_put(struct rddma_fabric_address *rfa)
{
	if ( rfa ) _fabric_put(to_fabric_address(rfa));
}

static int fabric_register(struct rddma_location *loc)
{
	struct fabric_address *old = to_fabric_address(loc->address);
	struct fabric_address *fna;
	char *ndev_name;
	struct net_device *ndev = NULL;

	if (old->reg_loc)
		return -EEXIST;

	if ( (ndev_name = rddma_get_option(&loc->desc,"netdev=")) ) {
		if ( !(ndev = dev_get_by_name(ndev_name)) )
			return -ENODEV;
	}
	else if (old->ndev)
		ndev = old->ndev;
	else if ( !(ndev = dev_get_by_name(netdev_name)) )
		return -ENODEV;
	
	fna = find_fabric_address(loc->desc.offset,0,ndev);
	loc->address = &fna->rfa;
	_fabric_put(old);

	fna->reg_loc = rddma_location_get(loc);

	return 0;
}

static void fabric_unregister(struct rddma_location *loc)
{
	fabric_put(loc->address);
	rddma_location_put(loc);
}


static struct rddma_address_ops fabric_net_ops = {
	.transmit = fabric_transmit,
	.register_location = fabric_register,
	.unregister_location = fabric_unregister,
	.get = fabric_get,
	.put = fabric_put,
};

static int __init fabric_net_init(void)
{
	struct fabric_address *fna;
	if ( (fna = new_fabric_address(UNKNOWN_IDX,0,0)) ) {
		snprintf(fna->rfa.name, RDDMA_MAX_FABRIC_NAME_LEN, "%s", "rddma_fabric_net");
		if (netdev_name)
			if ( (fna->ndev = dev_get_by_name(netdev_name)) ) {
				rddma_packets.dev = fna->ndev;
				dev_add_pack(&rddma_packets);
				return rddma_fabric_register(&fna->rfa);
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

