 /* 
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
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/rio.h>
#include <linux/rio_drv.h>
#include <linux/rio_ids.h>
#include <linux/list.h>

#define RDDMA_DOORBELL_START 0x2000
#define RDDMA_DOORBELL_END 0x4000
#define RDDMA_DOORBELL_HASHLEN 256

#define RDDMA_FABRIC_MSG 0

static int first_probe = 1;

static char *netdev_name = "rionet"; /* "eth0" or "br0" or "bond0" "rionet0" etc */
module_param(netdev_name, charp, 0444);

static unsigned short netdev_type = 0xfeed;
module_param(netdev_type, ushort, 0444);

static unsigned long default_dest = 1;
module_param(default_dest,ulong,0444);

struct fabric_address {
	unsigned char hw_address[ETH_ALEN];
	unsigned long idx;	/* ip address hint hint nod nod */
	unsigned long src_idx;	/* src ip address */
	struct rddma_location *reg_loc;
#define UNKNOWN_IDX 0UL
	struct net_device *ndev;
	struct list_head list;
	struct rddma_fabric_address rfa;
	struct kobject kobj;
	u16 rio_id;
};

#define is_rddma_rionet_capable(src_ops,dst_ops) \
	((src_ops & RIO_SRC_OPS_DOORBELL) && \
	 (dst_ops & RIO_DST_OPS_DOORBELL))

static struct rio_mport *port;

/* Jimmy hack: Following symbol is exported so the DMA driver can
 * dump RIO registers 
 */
struct rio_mport *rddma_rio_port;

struct event_node {
	struct list_head node;
	void (*cb) (void *);
	void *arg;
	int id;
};

struct _rddma_event_mgr {
	int next;
	int mindepth;
	int first_id;
	int last_id;
	int num_alloc;
	int num;
	int high_watermark;
	int hashlen;
	spinlock_t lock;
	struct semaphore sem;
	struct event_node *event_harray;
};

static struct _rddma_event_mgr *dbmgr;
static int find_db_in_list(struct list_head *q, int val, struct event_node **db);
static void rddma_events_uninit(struct _rddma_event_mgr *evmgr);
static struct _rddma_event_mgr *rddma_events_init(int first, int last, int hashlen);

static void rddma_dbell_event(struct rio_mport *mport, void *dev_id, u16 src, 
	u16 dst, u16 id);
static int doorbell_send (struct rddma_fabric_address *address, int id);
static int rddma_get_doorbell (void (*cb)(void *),void * arg);
static void rddma_put_doorbell (int id);

static inline struct fabric_address *to_fabric_address(struct rddma_fabric_address *rfa)
{
	return rfa ? container_of(rfa,struct fabric_address,rfa) : NULL;
}

int get_rio_id (struct rddma_fabric_address *rfa)
{
	struct fabric_address *a = to_fabric_address (rfa);
	if (a)
		return (a->rio_id);
	else
		return -1;
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
		fp->rio_id = hwaddr[5];
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

static struct rddma_address_ops fabric_rionet_ops;

static struct fabric_address *new_fabric_address(unsigned long idx, unsigned long src_idx, char *hwaddr, struct net_device *ndev)
{
	struct fabric_address *new = kzalloc(sizeof(struct fabric_address),GFP_KERNEL);
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);

	INIT_LIST_HEAD(&new->list);

	new->kobj.ktype = &fabric_address_type;
	kobject_init(&new->kobj);

	new->idx = idx;

	update_fabric_address(new,src_idx,hwaddr,ndev);

	new->rfa.ops = &fabric_rionet_ops;
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
	__be32          h_pkt_info;
} __attribute__((packed));

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

	if (skb_tailroom(skb))  /* FIXME */
		*skb_put(skb,1) = '\0';

	return rddma_fabric_receive(&fna->rfa, skb);

forget:
	_fabric_put(fna);
	kfree_skb(skb);
	return NET_RX_DROP;
}

struct packet_type rddma_packets = {
	.func = rddma_rx_packet,
};

static int fabric_transmit(struct rddma_fabric_address *addr, struct sk_buff *skb)
{
	struct rddmahdr *mac;
	unsigned long srcidx = 0, dstidx = 0;

	int ret = NET_XMIT_DROP;
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
	return ret;
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
	if (old) {
		fna->rio_id = old->rio_id;
	}
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

static struct rddma_address_ops fabric_rionet_ops = {
	.transmit = fabric_transmit,
	.register_location = fabric_register,
	.unregister_location = fabric_unregister,
	.get = fabric_get,
	.put = fabric_put,
	.doorbell = doorbell_send,
	.register_doorbell = rddma_get_doorbell,
	.unregister_doorbell = rddma_put_doorbell,
};

#if 1

static struct rio_device_id rionet_id_table[] = {
	{RIO_DEVICE(RIO_ANY_ID,RIO_ANY_ID)}
};

static int fabric_rionet_probe(struct rio_dev *rdev, const struct rio_device_id *id);
static void fabric_rionet_remove(struct rio_dev *rdev);

static struct rio_driver rddma_rio_drv = {
	.name = "rddma_rionet",
	.id_table = rionet_id_table, 
	.probe = fabric_rionet_probe,
	.remove = fabric_rionet_remove,
};

#endif


static void fabric_rionet_remove(struct rio_dev *rdev)
{
	/* dummy for now */
}

#define is_rddma_rionet_capable(src_ops,dst_ops) \
	((src_ops & RIO_SRC_OPS_DOORBELL) && \
	 (dst_ops & RIO_DST_OPS_DOORBELL))

static int  fabric_rionet_probe(struct rio_dev *rdev, 
	const struct rio_device_id *id)
{
	struct fabric_address *fna;
	int rc;
	int src_ops;
	int dst_ops;

	if (!first_probe)
		return 0;

	/* Set up for doorbells over RIO */

	first_probe = 1;
	port = rdev->net->hport;
rddma_rio_port = port;
	rio_local_read_config_32(port, RIO_SRC_OPS_CAR, &src_ops);
	rio_local_read_config_32(port, RIO_DST_OPS_CAR, &dst_ops);
	if (!is_rddma_rionet_capable(src_ops, dst_ops)) {
		printk("Unable to use rionet fabric -- no doorbell capability\n");;
		return -EINVAL;
	}

	/* Attach event handler to doorbell interrupt */
	if ((rc = rio_request_inb_dbell(port, (void *) port,
					RDDMA_DOORBELL_START,
					RDDMA_DOORBELL_END,
					rddma_dbell_event)) < 0)
		return -EINVAL;

	dbmgr = rddma_events_init(RDDMA_DOORBELL_START, RDDMA_DOORBELL_END, RDDMA_DOORBELL_HASHLEN);

	/* Messages over ethernet */

	if ( (fna = new_fabric_address(UNKNOWN_IDX,0,0,0)) ) {
		snprintf(fna->rfa.name, RDDMA_MAX_FABRIC_NAME_LEN, "%s", "rddma_fabric_rionet");
		if (netdev_name)
			if ( (fna->ndev = dev_get_by_name(netdev_name)) ) {
				rddma_packets.dev = fna->ndev;
				rddma_packets.type = htons(netdev_type);
				fna->rio_id = port->id;
				dev_add_pack(&rddma_packets);
				return rddma_fabric_register(&fna->rfa);
			}
		return -ENODEV;
	}
	return -ENOMEM;
}

/* RDDMA events for rionet are a range of 16-bit values associated with
 * a callback function and an argument.  The H/W implementation is RIO doorbells.
 * When an event is received over the fabric, the doorbell unit
 * raises an interrupt callback function is executed.
 *
 * Usage:
 *
 * 1) struct _rddma_event_mgr *rddma_events_init(int first, int last, int hashlen)
 *
 *    Initialize a new event manager.  Supply the first and last event number, and also
 *    the length of the hash table that is used to store allocated events;
 *    Funtion returns event manager handle.
 * 
 * 2) int rddma_get_event (struct _rddma_event_mgr *evmgr, void (*cb)(void *),void * arg)
 *
 *    Supply an event manager handle,  callback function and argument (NULL is valid).  
 *    Funtion returns event id.
 *
 * 3) void rddma_event_dispatch(struct _rddma_event_mgr *evmgr, int id) 
 *
 *    Supply an event manager handle and id.
 *    Funtion will call the event's associated callback function.
 *
 * 4) int rddma_put_event(struct _rddma_event_mgr *evmgr, int id)
 *
 *    Supply an event manager handle and id.
 *    Funtion will free the event
 *
 * 5) void rddma_events_uninit(struct _rddma_event_mgr *evmgr)
 *
 *    Supply an event manager handle.
 *    Funtion will delete the event manager.
 *
 * Allocated events, along with their callback functions, 
 * are stored in event_harray[].  If the number of allocated
 * events exceeds the size of event_harray, array entries
 * become hash lists.
 *
 * Events are allocated in order from the shortest hash list.
 * If list_head.next of the event array is 0, the bin is unused.
 */


/** Event manager **/

/* Look for 'val' in linked list.  Return value of function is position in
 * list (0 if not found) 
 * 'db' is for returning pointer to found entry  */
static int find_db_in_list(struct list_head *q, int val, struct event_node **db)
{
	struct list_head *dnode;
	int queue_depth = 0;
	__list_for_each(dnode, q) {
		queue_depth++;
		*db = list_entry(dnode, struct event_node, node);
		if ((*db)->id == val)
			return queue_depth;
	}
	*db = NULL;
	return 0;
}

static int list_len (struct list_head *list)
{
	struct list_head *pos;
	int ret = 0;
	__list_for_each(pos, list) {
		ret++;
	}
	return ret;
}
void rddma_events_uninit(struct _rddma_event_mgr *evmgr)
{
	int i;
	struct list_head *temp;
	struct list_head *curr;
	struct event_node *pdb;

	for (i = 0; i < evmgr->hashlen; i++) {
		pdb = &evmgr->event_harray[i];
		if (pdb->node.next != NULL) {
			list_for_each_safe(curr, temp, &pdb->node) {
				kfree(list_entry(curr, struct event_node, node));
			}
		}
	}
	kfree (evmgr->event_harray);
	kfree (evmgr);
	
}

static struct _rddma_event_mgr *rddma_events_init(int first, int last, int hashlen)
{
	int num = last - first + 1;
	int i;
        struct _rddma_event_mgr *evmgr;
	evmgr = kmalloc(sizeof (struct _rddma_event_mgr), GFP_KERNEL);
	if (evmgr == NULL)
		return NULL;
	evmgr->event_harray = kmalloc (hashlen * sizeof (struct event_node), GFP_KERNEL);
	if (evmgr->event_harray == NULL)
		goto bad1;
	for (i = 0; i < hashlen; i++) {
		memset(&evmgr->event_harray[i], 0, sizeof(struct event_node));
		evmgr->event_harray[i].id = -1;
	}
	spin_lock_init (&evmgr->lock);
	init_MUTEX(&evmgr->sem);
	evmgr->next = 0;
	evmgr->mindepth = 0;
	evmgr->num = num;
	evmgr->num_alloc = 0;
	evmgr->high_watermark = 0;
	evmgr->first_id = first;
	evmgr->last_id = last;
	evmgr->hashlen = hashlen;
	RDDMA_DEBUG(MY_DEBUG,"%s, doorbell init ok\n",__FUNCTION__);
	return evmgr;
bad1:
	RDDMA_DEBUG(MY_DEBUG,"%s, doorbell init failed\n",__FUNCTION__);
	kfree(evmgr);
	return (NULL);
};

/* Dispatch event to the appropriate callback */
/* Note: this function called from interrupt level by rionet and rio fabric driver */

static void rddma_event_dispatch(struct _rddma_event_mgr *evmgr, int id) 
{
	int depth;
	int bin = (id - evmgr->first_id) % evmgr->hashlen;
	struct event_node *pevent = &evmgr->event_harray[bin];

	if (pevent->node.next == 0)
		depth = -1;
	else {
		if (pevent->id == id)
			depth = 0;
		else {
			depth = find_db_in_list(&pevent->node, id, &pevent);
			if (depth == 0)
				depth = -1;
		}
	}
	if (depth == -1) {
		RDDMA_DEBUG(MY_DEBUG,"%s, doorbell not found!\n",__FUNCTION__);
		return;
	}

	if (pevent->cb)
		(pevent->cb)(pevent->arg);
}

/* Allocate an event and associate it with a callback.
 * Only incoming events need callback... 
 * NULL callback function is OK.
 */
static int rddma_get_event (struct _rddma_event_mgr *evmgr, void (*cb)(void *),void * arg)
{
	int ret = -1;
	int id;
	int bin;
	int bindepth;
	int depth;
	int i;
	struct event_node *pevent;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	/* 'next' is bin number of next available event.
	 * It's -1 if all events allocated.
	 */
	if (evmgr->next == -1)
		return ret;

	down(&evmgr->sem);
	bin = evmgr->next;
	if (evmgr->mindepth != 0) { 
		/* Allocate an event node */
		pevent = (struct event_node *) kzalloc (sizeof(struct event_node),
			GFP_KERNEL);
		if (pevent == NULL) { /* Memory error */
			up(&evmgr->sem);
			return ret;
		}
		/* Find another number that fits on this chain */
		for (id = evmgr->next + evmgr->first_id; id <= evmgr->last_id; id += evmgr->hashlen) {
			if (id != evmgr->event_harray[bin].id) {
				struct event_node *temp;
				depth = find_db_in_list(&evmgr->event_harray[bin].node, id,
			       		&temp);
				if (depth)
					continue;
				else
					goto end_search;
			}
		}
end_search:
		if (id > evmgr->last_id) {
			evmgr->next = -1; /* All event numbers allocated */
			up(&evmgr->sem);
			kfree (pevent);
			return ret;
		}
		pevent->id = -1;
		list_add_tail(&pevent->node, &evmgr->event_harray[bin].node);
	}
	else {
		/* Simple case, don't need linked list */
		id = (int) (bin + evmgr->first_id);
		pevent = &evmgr->event_harray[bin];
		pevent->id = -1;
		INIT_LIST_HEAD(&pevent->node);
	}
	pevent->cb = cb;
	pevent->arg = arg;
	pevent->id = id;
	RDDMA_DEBUG(MY_DEBUG,"%s, doorbell = %d\n",__FUNCTION__, id);
	ret = id;
	evmgr->num_alloc++;
	if (evmgr->num_alloc > evmgr->high_watermark)
		evmgr->high_watermark = evmgr->num_alloc;

	if (evmgr->num <= evmgr->hashlen) {
		/* Scan till finding an empty bin, otherwise next = -1 */
		for (i = bin + 1; i < evmgr->num; i++) {
			if (evmgr->event_harray[i].node.next == NULL) {
				evmgr->next = i;
				up(&evmgr->sem);
				return (ret);
			}
		}
		evmgr->next = -1;
		up(&evmgr->sem);
		return (ret);
	}

	/* Find new 'next' (index of the shortest bin) */
	evmgr->next = (bin + 1) % evmgr->hashlen;
	pevent = &evmgr->event_harray[evmgr->next];
	for (i = evmgr->next; i < evmgr->hashlen; i++) {
		if (pevent->node.next == NULL)
			goto done; /* empty bin */
		else
			bindepth = list_len(&pevent->node) + 1;

		if (bindepth > evmgr->mindepth) {
			pevent++;
			continue;
		}
		else
			goto done;
	}
	/* find new minimum bin depth */
	while (1) {
		evmgr->mindepth++;
		pevent = &evmgr->event_harray[0];
		for (i = 0; i < evmgr->hashlen; i++) {
			bindepth = list_len(&pevent->node) + 1;
			if (bindepth == evmgr->mindepth)
				goto done;
			else
				pevent++;
		}
	}
done:
	evmgr->next = i;

	up(&evmgr->sem);
	return ret;
}

int rddma_put_event(struct _rddma_event_mgr *evmgr, int id)
{
	int bin = (id - evmgr->first_id) % evmgr->hashlen;
	int depth;
	unsigned long flags;
	struct event_node *pdb;
	void *node_free = NULL;

	if ((id < evmgr->first_id) || (id > evmgr->last_id))
		return -1;

	down(&evmgr->sem);

	if (evmgr->event_harray[bin].node.next == 0)
		depth = -1;
	else {
		depth = find_db_in_list(&evmgr->event_harray[bin].node, id, &pdb);
		if (depth == 0) {
			if (evmgr->event_harray[bin].id != id)
				depth = -1;
		}
	}
	if (depth == -1) {
		/* attempted to free unallocated doorbell */
		up(&evmgr->sem);
		return -1;
	}
	/* Doorbell in list 'bin' at 'depth' */
	/* If depth > 0, remove node from list and free it */

	spin_lock_irqsave(&evmgr->lock, flags);
	if (depth > 0) {
		list_del(&pdb->node);
		node_free = pdb;
	}
	else { 
		if (list_empty(&evmgr->event_harray[bin].node)) {
			/* Clear this array element */
			evmgr->event_harray[bin].node.next = NULL;
			evmgr->event_harray[bin].id = -1;
		}
		else {
			/* Move head of linked list into hash array */
			pdb = list_entry (evmgr->event_harray[bin].node.next, 
				struct event_node, node);
			evmgr->event_harray[bin].id = pdb->id;
			evmgr->event_harray[bin].cb = pdb->cb;
			evmgr->event_harray[bin].arg = pdb->arg;
			list_del(&pdb->node);
			node_free = pdb;
		}
	}

	spin_unlock_irqrestore(&evmgr->lock, flags);

	/* Compute depth of bin */
	if (evmgr->event_harray[bin].node.next == NULL)
		depth = 0;
	else {
		depth = 1 + list_len(&evmgr->event_harray[bin].node);
	}

	if (depth == evmgr->mindepth) {
		if (bin < evmgr->next)
			evmgr->next = bin;
	} 
	else if (depth < evmgr->mindepth) {
		evmgr->mindepth = depth;
		evmgr->next = bin;
	}
	--evmgr->num_alloc;

	up(&evmgr->sem);

	if (node_free)
		kfree(node_free);

	return 0;
}

/**** End of event manager *****/

static void rddma_doorbells_uninit(void)
{
	/* Disconnect from H/W layer */
	rio_release_inb_dbell(port, RDDMA_DOORBELL_START, RDDMA_DOORBELL_END);
	/* Tear down the doorbell manager */
	if (dbmgr)
		rddma_events_uninit(dbmgr);
	dbmgr = NULL;
}

/* Dispatch doorbell interrupt to the appropriate callback */
/* Note: this function called from interrupt level by RIO driver */
static void rddma_dbell_event(struct rio_mport *mport, void *dev_id, u16 src, 
	u16 dst, u16 id) 
{

	RDDMA_DEBUG(MY_DEBUG,"%s src=%d dst=%d bell=%d\n",__FUNCTION__,src,dst,id);
	rddma_event_dispatch(dbmgr, (int) id);
}

static int doorbell_send (struct rddma_fabric_address *address, int info)
{
	struct fabric_address *fa = to_fabric_address(address);
	RDDMA_DEBUG(MY_DEBUG,"%s dst=%d bell=%d\n",__FUNCTION__,fa->rio_id,info);
	rio_mport_send_doorbell(port, fa->rio_id, (u16) info);
	return 0;
}

/* Allocate a doorbell and associate it with a callback.
 * Only incoming doorbells need callback... 
 * NULL callback function is OK.
 */
static int rddma_get_doorbell (void (*cb)(void *),void * arg)
{

	return (rddma_get_event(dbmgr, cb, arg));

}

static void rddma_put_doorbell(int id)
{
	RDDMA_DEBUG(MY_DEBUG,"%s, doorbell = %d\n",__FUNCTION__, id);
	rddma_put_event(dbmgr, id);
	return;
}

/* End of doorbell manager */

static void __exit fabric_rionet_close(void)
{
	dev_remove_pack(&rddma_packets);
	rddma_doorbells_uninit();
	rddma_fabric_unregister("rddma_fabric_rionet");
}

static int __init fabric_rionet_init(void)
{
	return (rio_register_driver(&rddma_rio_drv));
}

EXPORT_SYMBOL(rddma_rio_port);
EXPORT_SYMBOL(get_rio_id);
module_init(fabric_rionet_init);
module_exit(fabric_rionet_close);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Terry <pterry@micromemroy.com>");
MODULE_DESCRIPTION("Implements an ethernet frame/rio doorbell based transport for RDDMA");
