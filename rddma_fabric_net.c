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

#define MY_DEBUG      RDDMA_DBG_FABNET | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_FABNET | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_fabric.h>
#include <linux/rddma_location.h>

#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/if_ether.h>

static char *netdev_name = "eth0"; /* "eth0" or "br0" or "bond0" "rionet0" etc */
module_param(netdev_name, charp, 0444);

static unsigned short netdev_type = 0xfeed;
module_param(netdev_type, ushort, 0444);

static unsigned long default_dest = 1;
module_param(default_dest,ulong,0444);

struct fabric_address {
	unsigned char hw_address[ETH_ALEN]; /* dest mac address */
	unsigned long idx;	/* dest ip address hint hint nod nod */
	unsigned long src_idx;	/* src ip address */
	struct rddma_location *reg_loc;
#define UNKNOWN_IDX 0UL
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
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s entered addr=%p\n",__FUNCTION__,old);
	if (old) {
		kfree(old);
	}
}

static struct kobj_type fabric_address_type = {
	.release = fabric_address_release,
};

static inline struct fabric_address *_fabric_get(struct fabric_address *fna)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s entered addr=%p\n",__FUNCTION__,fna);
	return (fna ? kobject_get(&fna->kobj), fna : NULL) ;
}

static inline void _fabric_put(struct fabric_address *fna)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s entered addr=%p\n",__FUNCTION__,fna);
	if ( fna ) kobject_put(&fna->kobj);
}

static struct fabric_address *address_table[256];

static void update_fabric_address(struct fabric_address *fp, unsigned long src_idx, char *hwaddr, struct net_device *ndev)
{
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %lx %s\n",__FUNCTION__,fp,hwaddr,src_idx,ndev->name);
	if (hwaddr) {
		RDDMA_DEBUG(MY_DEBUG,"%s " MACADDRFMT "\n",__FUNCTION__,MACADDRBYTES(hwaddr));
		memcpy(fp->hw_address,hwaddr,ETH_ALEN);
	}
	
	if (ndev) {
		RDDMA_DEBUG_SAFE(MY_DEBUG,fp->ndev,"%s overwriting old ndev %p with %p\n",__FUNCTION__,fp->ndev,ndev);
		fp->ndev = ndev;
	}

	if (src_idx) {
		RDDMA_DEBUG_SAFE(MY_DEBUG,fp->src_idx,"%s overwriting old src_idx %lx with %lx\n",__FUNCTION__,fp->src_idx,src_idx);
		fp->src_idx = src_idx;
	}
}

static struct rddma_address_ops fabric_net_ops;

static struct fabric_address *new_fabric_address(unsigned long idx, unsigned long src_idx, char *hwaddr, struct net_device *ndev)
{
	struct fabric_address *new = kzalloc(sizeof(struct fabric_address),GFP_KERNEL);
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);

	INIT_LIST_HEAD(&new->list);

	new->kobj.ktype = &fabric_address_type;
	kobject_init(&new->kobj);

	new->idx = idx;

	update_fabric_address(new,src_idx,hwaddr,ndev);

	new->rfa.ops = &fabric_net_ops;
	new->rfa.owner = THIS_MODULE;

	return new;
}


static struct fabric_address *find_fabric_mac(char *hwaddr, struct net_device *ndev)
{
	int i;
	struct fabric_address *fp, *new;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	for (i = 0, fp = address_table[i]; i < 16 ; i++, fp = address_table[i]) {
		if (!fp)
			continue;

		if (!memcmp(fp->hw_address,hwaddr,ETH_ALEN))
			return _fabric_get(fp);
		
		if (!list_empty(&fp->list)) {
			list_for_each_entry(new, &fp->list, list) {
				if (!memcmp(new->hw_address,hwaddr,ETH_ALEN))
					return _fabric_get(new);
			}
		}
				
	}
	return NULL;
}

static struct fabric_address *find_fabric_address(unsigned long idx, unsigned long src_idx, char *hwaddr, struct net_device *ndev)
{
	struct fabric_address *fp = address_table[idx & 15];
	struct fabric_address *new;

	RDDMA_DEBUG(MY_DEBUG,"%s %lx\n",__FUNCTION__,idx);
	RDDMA_DEBUG_SAFE(MY_DEBUG,hwaddr,"%s " MACADDRFMT "\n",__FUNCTION__,MACADDRBYTES(hwaddr));
	if ( idx == UNKNOWN_IDX) {
		if ( (new = find_fabric_mac(hwaddr,ndev)) )
			return new;
		else
			return new_fabric_address(idx,0,hwaddr,ndev);
	}

	if (fp) {
		if (fp->idx == idx) {
			update_fabric_address(fp,src_idx,hwaddr,ndev);
			return _fabric_get(fp);
		}

		if (!list_empty(&fp->list)) {
			list_for_each_entry(new, &fp->list , list) {
				if (new->idx == idx) {
					update_fabric_address(new,src_idx,hwaddr,ndev);
					return _fabric_get(new);
				}
			}
		}

		new = new_fabric_address(idx,src_idx,hwaddr,ndev);
		list_add_tail(&new->list,&fp->list);
		return _fabric_get(new);
	}

	address_table[idx & 15] = fp = _fabric_get(new_fabric_address(idx,src_idx,hwaddr,ndev));
	return fp;
}

static void remove_fabric_address(struct fabric_address *addr)
{
	struct fabric_address *hp = address_table[addr->idx & 15];
	struct fabric_address *np, *fp;
	RDDMA_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,addr);
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

struct db_node {
	void (*callback)(void *);
	void *var;
	struct db_node *next;
	int depth;
};

#define EVENT_SIZE 16
#define HASH_SIZE 8
#define HASH_MASK  ((1 << HASH_SIZE) - 1)
#define DEPTH_MASK ((1 << (EVENT_SIZE - HASH_SIZE)) - 1)
#define HASH(e) ((e) & HASH_MASK)
#define DEPTH(e) (((e) >> HASH_SIZE) & DEPTH_MASK)
#define EVNT(d,h) ((((d) & DEPTH_MASK) << HASH_SIZE) | ((h) & HASH_MASK))
#define MAX_EVENT (1<<EVENT_SIZE)
#define MAX_HASH (1<<HASH_SIZE)

static struct db_node *bell_hash[MAX_HASH];

static struct semaphore dbell_sem;

static struct db_node *find_db(int evnt)
{
	int hash = HASH(evnt);
	int depth = DEPTH(evnt);
	struct db_node *db = bell_hash[hash];

	while (db && db->depth <= depth)
		if (db->depth == depth)
			return db;
		else
			db = db->next;
	return NULL;
}

static void invoke_db(int evnt)
{
	struct db_node *db;

	db = find_db(evnt);
	if (db)
		db->callback(db->var);
}

static int allocate_db(void (*callback)(void *), void *var)
{
	struct db_node *db = kzalloc(sizeof(*db), GFP_KERNEL);
	int evnt = 0;
	struct db_node *odb;
	int depth;
	int hash;

	db->callback = callback;
	db->var = var;

	down(&dbell_sem);
	while (evnt < MAX_EVENT) {
		if (find_db(evnt)) {
			evnt++;
			continue;
		}

		hash = HASH(evnt);
		depth = DEPTH(evnt);
		db->depth = depth;

		if (depth) {
			odb = find_db(evnt - MAX_HASH);
			db->next = odb->next;
			odb->next = db;
		}
		else {
			db->next = NULL;
			bell_hash[hash] = db;
		}
		up(&dbell_sem);
		return evnt;
	}

	up(&dbell_sem);
	kfree(db);
	return -1;
}

static void deallocate_db(int evnt)
{
	struct db_node *db;
	struct db_node *odb;
	int hash;
	int depth;
	
	down(&dbell_sem);
	db = find_db(evnt);
	hash = HASH(evnt);
	depth = DEPTH(evnt);

	if (depth) {
		odb = find_db(evnt - MAX_HASH);
		odb->next = db->next;
	}
	else {
		bell_hash[hash] = db->next;
	}
	up(&dbell_sem);
	kfree(db);
}

static int doorbell_register(void (*callback)(void *), void *var)
{
	return allocate_db(callback,var);
}

static void doorbell_unregister(int doorbell)
{
	deallocate_db(doorbell);
}

/* 
 * Frame format dstmac,srcmac,rddmatype,dstidx,srcidx,string
 *                6       6       2        4      4      n
 *
 * All frames have srcmac.
 * dstmac broadcast means source doesn't know you but may be talking to you
 * dstmac unicast and yours means source is talking to you.
 * dstmac unicast and not you means drop and ignore.
 * srcidx UNKNOWN_IDX means I don't know who I am (bootp ;-)
 * dstidx UNKNOWN_IDX means I don't know who you are (hello syscontrol, dns, bootp, etc, ;-)
 *
 */

struct rddmahdr {
	unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
	unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
	__be16		h_proto;		/* packet type ID field	*/
	__be32          h_dstidx;
	__be32          h_srcidx;
	__be32          h_pkt_info; /* msg vs doorbell vs dma data */
} __attribute__((packed));

#define RDDMA_FABRIC_MSG   0
#define RDDMA_FABRIC_EVENT 1

#define RDDMA_FABRIC_IS_MSG(hpi) ((hpi) == 0)
#define RDDMA_FABRIC_IS_EVNT(hpi) (((hpi) & 0xffff) == 1)
#define RDDMA_FABRIC_IS_DATA(hpi) ((hpi) && (((hpi) & 0xffff) != 1))

#define RDDMA_FABRIC_GET_EVNT(hpi) (((hpi) >> 16) & 0xffff)
#define RDDMA_FABRIC_SET_EVNT(evnt) ((((evnt) & 0xffff) << 16) | 1)

static inline struct rddmahdr *rddma_hdr(struct sk_buff *skb)
{
	skb_pull(skb, (sizeof(struct rddmahdr) - sizeof(struct ethhdr)));
	return (struct rddmahdr *)eth_hdr(skb);
}

static int rddma_rx_packet(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
{
	struct fabric_address *fna;
	struct rddmahdr *mac = rddma_hdr(skb);
	unsigned long srcidx, dstidx;
	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

	dstidx = ntohl(mac->h_dstidx);
	srcidx = ntohl(mac->h_srcidx);

	fna = find_fabric_address(srcidx,0,mac->h_source,dev);
	
	if (fna->reg_loc)
		if (dstidx && fna->reg_loc->desc.extent != dstidx)
			goto forget;

	if (skb_tailroom(skb))	/* FIXME */
		*skb_put(skb,1) = '\0';

	if (RDDMA_FABRIC_IS_MSG(mac->h_pkt_info))
		return rddma_fabric_receive(&fna->rfa, skb);

	if (RDDMA_FABRIC_IS_EVNT(mac->h_pkt_info))
		invoke_db(RDDMA_FABRIC_GET_EVNT(mac->h_pkt_info));

forget:
	_fabric_put(fna);
	dev_kfree_skb(skb);
	return NET_RX_DROP;
}

struct packet_type rddma_packets = {
	.func = rddma_rx_packet,
};

static int fabric_transmit(struct rddma_fabric_address *addr, struct sk_buff *skb)
{
	struct rddmahdr *mac;
	unsigned long srcidx = 0, dstidx = 0;

	struct fabric_address *fna = to_fabric_address(addr);

	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %p " MACADDRFMT "\n",__FUNCTION__,addr,skb->data,fna, MACADDRBYTES(fna->hw_address));

	if (fna->ndev) {
		skb_reset_transport_header(skb);
		skb_reset_network_header(skb);
		mac  = (struct rddmahdr *)skb_push(skb,sizeof(struct rddmahdr));
		skb_reset_mac_header(skb);

		if (*((int *)(fna->hw_address)))
			memcpy(mac->h_dest,fna->hw_address,ETH_ALEN);
		else
			memcpy(mac->h_dest,fna->ndev->broadcast,ETH_ALEN);

		memcpy(mac->h_source, fna->ndev->dev_addr,ETH_ALEN);

		mac->h_proto = htons(netdev_type);

		mac->h_pkt_info = RDDMA_FABRIC_MSG;

		dstidx = htonl(fna->idx);
		srcidx = htonl(fna->src_idx);

/* 		if (!dstidx) */
/* 			dstidx = htonl(default_dest); */

		memcpy(&mac->h_srcidx, &srcidx, sizeof(mac->h_srcidx));
		memcpy(&mac->h_dstidx, &dstidx, sizeof(mac->h_dstidx));

		skb->dev = fna->ndev;
		skb->protocol = htons(netdev_type);
		skb->ip_summed = CHECKSUM_NONE;
		_fabric_put(fna);
		
		RDDMA_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,skb->data);

		return dev_queue_xmit(skb);
	}

	_fabric_put(fna);
	dev_kfree_skb(skb);
	return NET_XMIT_DROP;
}

static int send_doorbell(struct rddma_fabric_address *addr, int db)
{
	struct rddmahdr *mac;
	unsigned long srcidx = 0, dstidx = 0;
	struct sk_buff *skb;

	struct fabric_address *fna = to_fabric_address(addr);

	RDDMA_DEBUG(MY_DEBUG,"%s %p %x %p " MACADDRFMT "\n",__FUNCTION__,addr,db,fna, MACADDRBYTES(fna->hw_address));
	
	if ((skb = dev_alloc_skb(1024))) {
		if (fna->ndev) {
			skb_reserve(skb,128);
			skb_reset_transport_header(skb);
			skb_reset_network_header(skb);
			mac  = (struct rddmahdr *)skb_push(skb,sizeof(struct rddmahdr));
			skb_reset_mac_header(skb);

			if (*((int *)(fna->hw_address)))
				memcpy(mac->h_dest,fna->hw_address,ETH_ALEN);
			else
				memcpy(mac->h_dest,fna->ndev->broadcast,ETH_ALEN);

			memcpy(mac->h_source, fna->ndev->dev_addr,ETH_ALEN);

			mac->h_proto = htons(netdev_type);

			mac->h_pkt_info = RDDMA_FABRIC_SET_EVNT(db);

			dstidx = htonl(fna->idx);
			srcidx = htonl(fna->src_idx);

			memcpy(&mac->h_srcidx, &srcidx, sizeof(mac->h_srcidx));
			memcpy(&mac->h_dstidx, &dstidx, sizeof(mac->h_dstidx));

			skb->dev = fna->ndev;
			skb->protocol = htons(netdev_type);
			skb->ip_summed = CHECKSUM_NONE;
			_fabric_put(fna);
		
			RDDMA_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,skb->data);

			return dev_queue_xmit(skb);
		}

		_fabric_put(fna);
		dev_kfree_skb(skb);
	}
	return NET_XMIT_DROP;
}

static struct rddma_fabric_address *fabric_get(struct rddma_fabric_address *rfa)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s entered rfa=%p\n",__FUNCTION__,rfa);
	return rfa ? _fabric_get(to_fabric_address(rfa)), rfa : NULL;
}

static void fabric_put(struct rddma_fabric_address *rfa)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s entered rfa=%p\n",__FUNCTION__,rfa);
	if ( rfa ) _fabric_put(to_fabric_address(rfa));
}

static int fabric_register(struct rddma_location *loc)
{
	struct fabric_address *old = to_fabric_address(loc->desc.address);
	struct fabric_address *fna;
	char *ndev_name;
	struct net_device *ndev = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

/* 	if (old && old->reg_loc) */
/* 		return -EEXIST; */

	if ( (ndev_name = rddma_get_option(&loc->desc,"netdev=")) ) {
		if ( !(ndev = dev_get_by_name(ndev_name)) )
			return -ENODEV;
	}
	else if (old && old->ndev)
		ndev = old->ndev;
	else if ( !(ndev = dev_get_by_name(netdev_name)) )
		return -ENODEV;
	
	fna = find_fabric_address(loc->desc.offset,loc->desc.extent,0,ndev);
	loc->desc.address = &fna->rfa;
	fna->reg_loc = rddma_location_get(loc);

	rddma_location_put(old->reg_loc);
	_fabric_put(old);

	return 0;
}

static void fabric_unregister(struct rddma_location *loc)
{
	struct fabric_address *old = to_fabric_address(loc->desc.address);
	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);
	old->reg_loc = NULL;
	_fabric_put(old);
	rddma_location_put(loc);
}

static struct rddma_address_ops fabric_net_ops = {
	.transmit = fabric_transmit,
	.register_location = fabric_register,
	.unregister_location = fabric_unregister,
	.get = fabric_get,
	.put = fabric_put,
	.doorbell = send_doorbell,
	.register_doorbell = doorbell_register,
	.unregister_doorbell = doorbell_unregister,
};

static int __init fabric_net_init(void)
{
	struct fabric_address *fna;
	sema_init(&dbell_sem,1);
	if ( (fna = new_fabric_address(UNKNOWN_IDX,0,0,0)) ) {
		snprintf(fna->rfa.name, RDDMA_MAX_FABRIC_NAME_LEN, "%s", "rddma_fabric_net");
		if (netdev_name)
			if ( (fna->ndev = dev_get_by_name(netdev_name)) ) {
				rddma_packets.dev = fna->ndev;
				rddma_packets.type = htons(netdev_type);
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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Terry <pterry@micromemroy.com>");
MODULE_DESCRIPTION("Implements an ethenet frame based transport for RDDMA");


