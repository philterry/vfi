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

#include <linux/rddma.h>
#include <linux/rddma_subsys.h>
#include <linux/rddma_location.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_smb.h>
#include <linux/rddma_fabric.h>

#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/version.h>

struct call_back_tag {
	struct sk_buff *rqst_skb;
	struct sk_buff *rply_skb;
	wait_queue_head_t wq;
	struct work_struct wo;
	struct rddma_fabric_address *sender;
	void *cb_data;
	struct call_back_tag *check;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void fabric_do_rqst(void *data)
{
	struct work_struct *wo = (struct work_struct *) data;
#else
static void fabric_do_rqst(struct work_struct *wo)
{
#endif
	int ret = 0;
	struct call_back_tag *cb = container_of(wo, struct call_back_tag, wo);
	RDDMA_DEBUG(1,"%s entered\n",__FUNCTION__);

	cb->rply_skb = dev_alloc_skb(2048);
	skb_reserve(cb->rply_skb,128);

	ret = do_operation(cb->rqst_skb->data,cb->rply_skb->data,1928);
	RDDMA_ASSERT(ret < 1928, "reply truncated need reply buffer bigger than 2048!");

	skb_put(cb->rply_skb,ret);

	dev_kfree_skb(cb->rqst_skb);

	ret = rddma_fabric_tx(cb->sender,cb->rply_skb);
	rddma_fabric_put(cb->sender);

	kfree(cb);
}

/* Downcalls via the location->address */

int __must_check rddma_fabric_tx(struct rddma_fabric_address *address, struct sk_buff *skb)
{
	int ret = -ENODEV;
	RDDMA_DEBUG(1,"%s entered address=%p\n",__FUNCTION__,address);

	RDDMA_SAFE_DEBUG(1,(address),"%s: address_ops=%p\n",__FUNCTION__,address->ops);

	if ( address && address->ops  ) {
		address = address->ops->get(address);
		if ( (ret = address->ops->transmit(address,skb)) ) {
			address->ops->put(address);
			dev_kfree_skb(skb);
		}
	} else
		dev_kfree_skb(skb);

	return ret;
}

int rddma_address_register(struct rddma_location *loc)
{
	int ret = -EINVAL;
	RDDMA_DEBUG(1,"%s entered\n",__FUNCTION__);

	if (loc && loc->desc.address && loc->desc.address->ops)
		ret = loc->desc.address->ops->register_location(loc);
	return ret;
}

void rddma_address_unregister(struct rddma_location *loc)
{
	RDDMA_DEBUG(1,"%s entered\n",__FUNCTION__);
	if (loc && loc->desc.address && loc->desc.address->ops)
		loc->desc.address->ops->unregister_location(loc);
}


struct sk_buff *rddma_fabric_call(struct rddma_location *loc, int to, char *f, ...)
{
	va_list ap;
	struct call_back_tag *cb = kzalloc(sizeof(struct call_back_tag),GFP_KERNEL);
	RDDMA_DEBUG(1,"%s entered\n",__FUNCTION__);
	if (cb) {
		struct sk_buff *skb = dev_alloc_skb(2048);
		skb_reserve(skb,128);
		if (skb) {
			cb->check = cb;
			cb->rply_skb = NULL;
			
			va_start(ap,f);
			skb_put(skb,vsprintf(skb->data,f,ap));
			va_end(ap);
			skb_put(skb,sprintf(skb->tail, "?request=%p",cb)); 
			RDDMA_DEBUG(1,"	%s: %s\n",__FUNCTION__, skb->data);
			
			if (rddma_fabric_tx(loc->desc.address, skb)) {
				kfree(cb);
				return NULL;
			}
				
		
			init_waitqueue_head(&cb->wq);

			if (wait_event_interruptible_timeout(cb->wq, (cb->rply_skb != NULL), to*HZ) == 0) {
				kfree(cb);
				return NULL;
			}

			skb = cb->rply_skb;
			kfree(cb);
			return skb;
		}
	}
	return NULL;
}

/* Upcalls */
int rddma_fabric_receive(struct rddma_fabric_address *sender, struct sk_buff *skb)
{
	char *msg = skb->data;
	struct call_back_tag *cb = NULL;
	char *buf;
	RDDMA_DEBUG(1,"%s entered\n",__FUNCTION__);

	if ((buf = strstr(msg,"reply="))) {
		if ( sscanf(buf,"reply=%p",&cb) ) {
			if (cb && cb->check == cb) {
					cb->rply_skb = skb;
					wake_up_interruptible(&cb->wq);
					return 0;
			}
		}
		sender->ops->put(sender);
		dev_kfree_skb(skb);
	}
	else if ((buf = strstr(msg,"request="))) {
		struct call_back_tag *cb = kzalloc(sizeof(struct call_back_tag),GFP_KERNEL);
		cb->rqst_skb = skb;
		cb->check = cb;
		if ( (cb->sender = rddma_fabric_get(sender)) ) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
			INIT_WORK(&cb->wo, fabric_do_rqst, (void *) &cb->wo);
#else
			INIT_WORK(&cb->wo, fabric_do_rqst);
#endif
			schedule_work(&cb->wo);
			return 0;
		}
		else {
			sender->ops->put(sender);
			dev_kfree_skb(skb);
			kfree(cb);
		}
	}
	sender->ops->put(sender);
	return -EINVAL;

}

static struct rddma_fabric_address *fabrics[RDDMA_MAX_FABRICS];

int rddma_fabric_register(struct rddma_fabric_address *addr)
{
	int ret = 0;
	int i;

	for (i = 0; i < RDDMA_MAX_FABRICS && fabrics[i] ; i++)
		if (!strcmp(addr->name, fabrics[i]->name) ) {
			ret = -EEXIST;
			break;
		}

	if ( i == RDDMA_MAX_FABRICS)
		ret = -ENOMEM;

	if (!ret)
		fabrics[i] = addr;

	RDDMA_SAFE_DEBUG(1,(addr),"%s ops=%p\n",__FUNCTION__,addr->ops);
	RDDMA_SAFE_DEBUG(1,(addr),"%s register %s returns %d\n",__FUNCTION__,addr->name,ret);
	return ret;
}

void rddma_fabric_unregister(const char *name)
{
	int i;
	RDDMA_DEBUG(1,"%s entered with %s\n",__FUNCTION__,name);

	for (i = 0; i < RDDMA_MAX_FABRICS && fabrics[i] ; i++)
		if (fabrics[i])
			if (!strcmp(name,fabrics[i]->name) ) {
				fabrics[i] = NULL;
				return;
			}
}

struct rddma_fabric_address *rddma_fabric_find(const char *name)
{
	int i;
	RDDMA_DEBUG(1,"%s entered with %s\n",__FUNCTION__, name);
	if (name)
		for (i = 0 ; i < RDDMA_MAX_FABRICS && fabrics[i]; i++)
			if (fabrics[i])
				if (!strcmp(name,fabrics[i]->name) ) {
					if (try_module_get(fabrics[i]->owner)) {
						fabrics[i]->ops->get(fabrics[i]);
						RDDMA_DEBUG(1,"\t%s returning with %s\n",__FUNCTION__, fabrics[i]->name);
						return fabrics[i];
					}
					RDDMA_DEBUG(1,"\t%s failed module_get\n",__FUNCTION__);
					return NULL;
				}
	RDDMA_DEBUG(1,"\t%s failed to find %s\n",__FUNCTION__,name);
	return NULL;
}

struct rddma_fabric_address *rddma_fabric_get(struct rddma_fabric_address *addr)
{
	RDDMA_DEBUG(1,"%s entered\n",__FUNCTION__);
	if (try_module_get(addr->owner)) {
		addr->ops->get(addr);
		return addr;
	}
	return NULL;
}

void rddma_fabric_put(struct rddma_fabric_address *addr)
{
	RDDMA_DEBUG(1,"%s entered\n",__FUNCTION__);
	addr->ops->put(addr);
	module_put(addr->owner);
}

EXPORT_SYMBOL(rddma_fabric_receive);
EXPORT_SYMBOL(rddma_fabric_register);
EXPORT_SYMBOL(rddma_fabric_unregister);
