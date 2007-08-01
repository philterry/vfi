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

struct call_back_tag {
	struct sk_buff *rqst_skb;
	struct sk_buff *rply_skb;
	wait_queue_head_t wq;
	struct work_struct wo;
	void *reply_cb;
	struct rddma_location *sender;
	void *cb_data;
	struct call_back_tag *check;
};

static int fabric_tx(struct rddma_location *loc, struct sk_buff *skb)
{
	int ret = 0;
	if ( (ret = loc->address.ops->fabric_transmit(&loc->address,skb)) ) {
		dev_kfree_skb(skb);
	}

	return ret;
}

static int fabric_tx_reply(struct rddma_location *loc, struct call_back_tag *cb)
{
	skb_put(cb->rply_skb,sprintf(cb->rply_skb->data, ",reply=%p",cb->reply_cb));

	return fabric_tx(loc,cb->rply_skb);
	
}

static void fabric_do_rqst(struct work_struct *wo)
{
	int ret = 0;
	struct call_back_tag *cb = container_of(wo, struct call_back_tag, wo);

	cb->rply_skb = dev_alloc_skb(2048);
	skb_reserve(cb->rply_skb,128);

	ret = do_operation(cb->rqst_skb->data,cb->rply_skb->data,1928);
	RDDMA_ASSERT(ret < 1928, "reply truncated need reply buffer bigger than 2048!");

	skb_put(cb->rply_skb,ret);

	dev_kfree_skb(cb->rqst_skb);

	fabric_tx_reply((struct rddma_location *)cb->sender,cb);

	kfree(cb);
}

int rddma_fabric_receive(struct rddma_location *sender, struct sk_buff *skb)
{
	char *msg = skb->data;
	struct call_back_tag *cb = NULL;
	char *buf;

	if ((buf = strstr(msg,"?reply="))) {
		if ( sscanf(buf,"?reply=%p",&cb) ) {
			if (cb && cb->check == cb)
					cb->rply_skb = skb;
					wake_up_interruptible(&cb->wq);
		}
	}
	else if ((buf = strstr(msg,"?request="))) {
		void *reply_cb;
		struct call_back_tag *cb = kzalloc(sizeof(struct call_back_tag),GFP_KERNEL);
		sscanf(buf,"?request=%p",&reply_cb);
		cb->rqst_skb = skb;
		cb->reply_cb = reply_cb;
		cb->check = cb;
		INIT_WORK(&cb->wo, fabric_do_rqst);
		schedule_work(&cb->wo);
	}
	else
		return -EINVAL;

	return 0;

}

struct sk_buff *rddma_fabric_call(struct rddma_location *loc, int to, char *f, ...)
{
	va_list ap;
	struct call_back_tag *cb = kzalloc(sizeof(struct call_back_tag),GFP_KERNEL);
	if (cb) {
		struct sk_buff *skb = dev_alloc_skb(2048);
		if (skb) {
			cb->check = cb;
			cb->rply_skb = NULL;
			
			va_start(ap,f);
			skb_put(skb,vsprintf(skb->data,f,ap));
			va_end(ap);
			skb_put(skb,sprintf(skb->data, "?request=%p",cb));
				
			fabric_tx(loc, skb);
		
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

int rddma_fabric_register(struct rddma_location *loc)
{
	return loc->address.ops->fabric_register(&loc->address);
}

void rddma_fabric_unregister(struct rddma_location *loc)
{
	loc->address.ops->fabric_unregister(&loc->address);
}
