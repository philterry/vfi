/* 
 * 
 * Copyright 2008 Vmetro
 * Phil Terry <pterry@vmetro.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define MY_DEBUG      VFI_DBG_FABRIC | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_FABRIC | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_FABRIC | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi.h>
#include <linux/vfi_subsys.h>
#include <linux/vfi_location.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_fabric.h>

#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/version.h>

struct call_back_tag {
	struct sk_buff *rqst_skb;
	struct sk_buff *rply_skb;
	wait_queue_head_t wq;
	struct work_struct wo;
	struct workqueue_struct *woq;
	struct vfi_fabric_address *sender;
	void *cb_data;
	struct call_back_tag *check;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void fabric_disposeq(void *data)
{
	struct call_back_tag *cb = (struct call_back_tag *) data;
#else
static void fabric_disposeq(struct work_struct *wo)
{
	struct call_back_tag *cb = container_of(wo, struct call_back_tag, wo);
#endif
	destroy_workqueue(cb->woq);
	kfree(cb);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void fabric_do_rqst(void *data)
{
	struct call_back_tag *cb = (struct call_back_tag *) data;
#else
static void fabric_do_rqst(struct work_struct *wo)
{
	struct call_back_tag *cb = container_of(wo, struct call_back_tag, wo);
#endif
	int ret = 0;
	int size = 1928;
	VFI_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

	cb->rply_skb = dev_alloc_skb(2048);
	skb_reserve(cb->rply_skb,128);
	VFI_DEBUG (MY_DEBUG, "> %s calls do_operation (...)\n", __func__);
	ret = do_operation(cb->rqst_skb->data,cb->rply_skb->data,&size);
	VFI_ASSERT(size < 1928, "reply truncated need reply buffer bigger than 2048!");

	dev_kfree_skb(cb->rqst_skb);
	
	if (!ret) {
		skb_put(cb->rply_skb,size);
		ret = vfi_fabric_tx(cb->sender,cb->rply_skb);
	}

	vfi_fabric_put(cb->sender);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	PREPARE_WORK(&cb->wo, fabric_disposeq, (void *) cb);
#else
	PREPARE_WORK(&cb->wo, fabric_disposeq);
#endif
	schedule_work(&cb->wo);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void fabric_sched_rqst(void *data)
{
	struct call_back_tag *cb = (struct call_back_tag *) data;
#else
static void fabric_sched_rqst(struct work_struct *wo)
{
	struct call_back_tag *cb = container_of(wo, struct call_back_tag, wo);
#endif
	cb->woq = create_workqueue("fab_rqst");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	PREPARE_WORK(&cb->wo, fabric_do_rqst, (void *) cb);
#else
	PREPARE_WORK(&cb->wo, fabric_do_rqst);
#endif
	queue_work(cb->woq,&cb->wo);
}

/* Downcalls via the location->address */

int __must_check vfi_fabric_tx(struct vfi_fabric_address *address, struct sk_buff *skb)
{
	int ret = -ENODEV;
	VFI_DEBUG(MY_DEBUG,"%s %p %s\n",__FUNCTION__,address,skb->data);

	VFI_DEBUG_SAFE(MY_DEBUG,(address),"%s: address_ops=%p\n",__FUNCTION__,address->ops);

	if ( address && address->ops  ) {
		address = address->ops->get(address);
		if ( (ret = address->ops->transmit(address,skb)) ) {
			dev_kfree_skb(skb);
		}
	} else
		dev_kfree_skb(skb);

	VFI_DEBUG(MY_DEBUG,"%s %d\n",__FUNCTION__,ret);
	return VFI_RESULT(ret);
}

int vfi_address_register(struct vfi_location *loc)
{
	int ret = -EINVAL;
	VFI_DEBUG(MY_DEBUG,"%s entered for \"%s.%s\"\n",__FUNCTION__, loc->desc.name, loc->desc.location);

	if (loc && loc->desc.address && loc->desc.address->ops)
		ret = loc->desc.address->ops->register_location(loc);
	return VFI_RESULT(ret);
}

void vfi_address_unregister(struct vfi_location *loc)
{
	VFI_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);
	if (loc && loc->desc.address && loc->desc.address->ops)
		loc->desc.address->ops->unregister_location(loc);
}

int vfi_doorbell_register(struct vfi_fabric_address *address, void (*callback)(void *), void *var)
{
	VFI_KTRACE ("<*** %s IN ***>\n", __func__);
	if (address->ops && address->ops->register_doorbell) {
		int ret = address->ops->register_doorbell(callback,var);
		VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
		return VFI_RESULT(ret);
	}
	VFI_KTRACE ("<*** xxx %s - did nothing, OUT ***>\n", __func__);
	return VFI_RESULT(-EINVAL);
}

void vfi_doorbell_unregister(struct vfi_fabric_address *address, int doorbell)
{
	VFI_KTRACE ("<*** %s doorbell %08x IN ***>\n", __func__, doorbell); 
	if (address && address->ops && address->ops->unregister_doorbell)
		address->ops->unregister_doorbell(doorbell);
	VFI_KTRACE ("<*** %s doorbell %08x OUT ***>\n", __func__, doorbell);
	
}

void vfi_doorbell_send(struct vfi_fabric_address *address, int doorbell)
{
	if (address && address->ops && address->ops->doorbell) {
		address->ops->doorbell(address, doorbell);
	}
	else {
		VFI_DEBUG (MY_DEBUG, "xxx %s address (%p), address->ops (%p), address->ops->doorbell (%p)\n", __func__, 
		        address, 
		        (address) ? address->ops : NULL,
		        (address && address->ops) ? address->ops->doorbell : NULL);
	}
}

int vfi_fabric_call(struct sk_buff **retskb, struct vfi_location *loc, int to, char *f, ...)
{
	int ret;
	va_list ap;
	struct call_back_tag *cb = kzalloc(sizeof(struct call_back_tag),GFP_KERNEL);
	VFI_DEBUG(MY_DEBUG,"%s entered - call \"%s.%s\"\n",__FUNCTION__, (loc) ? loc->desc.name : "<NULL>", (loc) ? loc->desc.location : "<NULL>");
	*retskb = NULL;
	if (cb) {
		struct sk_buff *skb = dev_alloc_skb(2048);
		if (skb) {
			skb_reserve(skb,128);
			cb->check = cb;
			cb->rply_skb = NULL;
			
			va_start(ap,f);
			skb_put(skb,vsprintf(skb->data,f,ap));
			va_end(ap);
			skb_put(skb,sprintf(skb->tail, "%crequest(%p)",strstr(skb->data,"?") ? ',' : '?', cb)); 
			VFI_DEBUG(MY_DEBUG,"\t%s: %s\n",__FUNCTION__, skb->data);
			
			vfi_address_register(loc);

			if ((ret = vfi_fabric_tx(loc->desc.address, skb))) {
				kfree(cb);
				VFI_DEBUG (MY_DEBUG, "xx\t%s: failed to transmit command over fabric!\n", __func__);
				return VFI_RESULT(ret);
			}
				
		
			init_waitqueue_head(&cb->wq);

			/* Jimmy hack!  Increased timeout value 20x for all
			 * ops other than finds 
			 */
			if (strstr(f,"_find:") == NULL)
				to *= 20;

			if (wait_event_interruptible_timeout(cb->wq, (cb->rply_skb != NULL), to*HZ) == 0) {
				kfree(cb);
				VFI_DEBUG (MY_DEBUG, "xx\t%s: TIMEOUT waiting for response!\n", __func__);
				return VFI_RESULT(-EIO);
			}

			skb = cb->rply_skb;
			VFI_DEBUG (MY_DEBUG, "--\t%s: reply [ %s ]\n",__FUNCTION__, skb->data);
			kfree(cb);
			*retskb = skb;
			return VFI_RESULT(0);
		}
		else {
			VFI_DEBUG (MY_DEBUG, "xx\t%s: no reply\n", __func__);
		}
	}
	else {
		VFI_DEBUG (MY_DEBUG, "xx\t%s failed to kzalloc a callback tag\n", __func__);
	}
	return VFI_RESULT(-ENOMEM);
}

/* Upcalls */
int vfi_fabric_receive(struct vfi_fabric_address *sender, struct sk_buff *skb)
{
	char *msg = skb->data;
	struct call_back_tag *cb = NULL;
	char *buf;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s %p %s\n",__FUNCTION__,sender,msg);

	if ((buf = strstr(msg,"reply("))) {
		VFI_DEBUG(MY_DEBUG,"%s %s\n",__FUNCTION__,buf);
		if ( (ret = sscanf(buf,"reply(%x)", (unsigned int *)&cb)) ) {
			VFI_DEBUG(MY_DEBUG,"%s cb(%p)\n",__FUNCTION__,cb);
			if (cb && cb->check == cb) {
				cb->check = 0;
				VFI_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,cb);
				cb->rply_skb = skb;
				wake_up_interruptible(&cb->wq);
				return VFI_RESULT(0);
			}
		}
		VFI_DEBUG(MY_DEBUG,"%s ret(%d) %p\n",__FUNCTION__,ret,cb);
	}
	else if ((buf = strstr(msg,"request("))) {
		struct call_back_tag *cb = kzalloc(sizeof(struct call_back_tag),GFP_KERNEL);
		cb->rqst_skb = skb;
		cb->check = cb;
		if ( (cb->sender = vfi_fabric_get(sender)) ) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
			INIT_WORK(&cb->wo, fabric_sched_rqst, (void *) cb);
#else
			INIT_WORK(&cb->wo, fabric_sched_rqst);
#endif
			schedule_work(&cb->wo);
			return VFI_RESULT(0);
		}
		else {
			kfree(cb);
		}
	}
	sender->ops->put(sender);
	dev_kfree_skb(skb);
	return VFI_RESULT(NET_RX_DROP);

}

static struct vfi_fabric_address *fabrics[VFI_MAX_FABRICS];

int vfi_fabric_register(struct vfi_fabric_address *addr)
{
	int ret = 0;
	int i;

	for (i = 0; i < VFI_MAX_FABRICS && fabrics[i] ; i++)
		if (!strcmp(addr->name, fabrics[i]->name) ) {
			ret = -EEXIST;
			break;
		}

	if ( i == VFI_MAX_FABRICS)
		ret = -ENOMEM;

	if (!ret)
		fabrics[i] = addr;

	VFI_DEBUG_SAFE(MY_DEBUG,(addr),"%s ops=%p\n",__FUNCTION__,addr->ops);
	VFI_DEBUG_SAFE(MY_DEBUG,(addr),"%s register %s returns %d\n",__FUNCTION__,addr->name,ret);
	return VFI_RESULT(ret);
}

void vfi_fabric_unregister(const char *name)
{
	int i;
	VFI_DEBUG(MY_DEBUG,"%s entered with %s\n",__FUNCTION__,name);

	for (i = 0; i < VFI_MAX_FABRICS && fabrics[i] ; i++)
		if (fabrics[i])
			if (!strcmp(name,fabrics[i]->name) ) {
				fabrics[i] = NULL;
				return;
			}
}

struct vfi_fabric_address *vfi_fabric_find(const char *name)
{
	int i;
	VFI_DEBUG(MY_DEBUG,"%s entered with %s\n",__FUNCTION__, name);
	if (name)
		for (i = 0 ; i < VFI_MAX_FABRICS && fabrics[i]; i++)
			if (fabrics[i])
				if (!strcmp(name,fabrics[i]->name) ) {
					if (try_module_get(fabrics[i]->owner)) {
						fabrics[i]->ops->get(fabrics[i]);
						VFI_DEBUG(MY_DEBUG,"\t%s returning with %s\n",__FUNCTION__, fabrics[i]->name);
						return fabrics[i];
					}
					VFI_DEBUG(MY_DEBUG,"\t%s failed module_get\n",__FUNCTION__);
					return NULL;
				}
	VFI_DEBUG(MY_DEBUG,"\t%s failed to find %s\n",__FUNCTION__,name);
	return NULL;
}

struct vfi_fabric_address *vfi_fabric_get(struct vfi_fabric_address *addr)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s entered addr=%p\n",__FUNCTION__,addr);
	if (addr)
		if (try_module_get(addr->owner)) {
			addr->ops->get(addr);
			return addr;
		}
	return NULL;
}

void vfi_fabric_put(struct vfi_fabric_address *addr)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s entered addr=%p\n",__FUNCTION__,addr);
	if (addr) {
		addr->ops->put(addr);
		module_put(addr->owner);
	}
}

EXPORT_SYMBOL(vfi_fabric_receive);
EXPORT_SYMBOL(vfi_fabric_register);
EXPORT_SYMBOL(vfi_fabric_unregister);
