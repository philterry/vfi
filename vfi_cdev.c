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

#define MY_DEBUG      VFI_DBG_CDEV | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_CDEV | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_CDEV | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi.h>

#include <linux/aio.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/proc_fs.h>

#include <linux/vfi_drv.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_mmap.h>
#include <linux/version.h>

struct proc_dir_entry *proc_root_vfi = NULL;

struct mybuffers {
	struct list_head list;
	int size;
	char *buf;
	char *reply;
};

#define to_mybuffers(lh) list_entry(lh, struct mybuffers, list)

struct privdata {
	struct semaphore sem;
	wait_queue_head_t rwq;
	struct list_head readlist;
	struct list_head seeklist;
	int seekbufs;
	loff_t pos;
	int open;
	struct mybuffers *mybuf;
	int offset;
	int size;
	struct kobject kobj;
};

static void release_privdata(struct kobject *kobj)
{
	struct privdata *priv = container_of(kobj, struct privdata, kobj);
	kfree(priv);
}

static struct kobj_type privtype = {
	.release = release_privdata,
};

static loff_t vfi_llseek(struct file *filep, loff_t offset, int origin)
{
	struct privdata *priv = (struct privdata *)filep->private_data;

	if (down_interruptible(&priv->sem))
		return -ERESTARTSYS;

	VFI_DEBUG(MY_ERROR,"%s filep(%p),offset(%lld),origin(%d)\n",__FUNCTION__,filep,offset,origin);

	switch (origin) {
	case 1:  
		if (offset < 0) {
			if (!list_empty(&priv->seeklist)) {
				list_move(priv->seeklist.next,&priv->readlist);
				priv->seekbufs--;
				offset = priv->pos += offset;
				break;
			}
			else VFI_DEBUG(MY_ERROR,"%s backward seek failed\n",__FUNCTION__);
		}
		else if (offset == 0) { 
			priv->pos = 0;
			break;
		}
		else VFI_DEBUG(MY_ERROR,"%s forward seek requested\n",__FUNCTION__);

		// fall through
	default: 
		offset = no_llseek(filep,offset,origin); 
		break;
	}

	up(&priv->sem);

	return offset;
}

static ssize_t vfi_read(struct file *filep, char __user *buf, size_t count, loff_t *offset)
{
	int ret;
	int mycount = 0;
	struct privdata *priv = (struct privdata *)filep->private_data;
	int left;
	int copied;

	if (down_interruptible(&priv->sem))
		return -ERESTARTSYS;

	VFI_DEBUG(MY_DEBUG,"%s buf(%p),count(%d),offset(%lld)\n",__FUNCTION__,buf,count,*offset);

	while (!mycount) {
		if (!priv->mybuf) {
			while (list_empty(&priv->readlist)) {
				up(&priv->sem);
				if (filep->f_flags & O_NONBLOCK)
					return -EAGAIN;
				if (wait_event_interruptible(priv->rwq, (!list_empty(&priv->readlist))))
					return -ERESTARTSYS;
				if (down_interruptible(&priv->sem))
					return -ERESTARTSYS;
			}
			priv->mybuf = to_mybuffers(priv->readlist.next);
			list_del(priv->readlist.next);
			priv->size = priv->mybuf->size;
			priv->offset = 0;
		}
		left = priv->size - priv->offset;
		copied = count < left ? count : left;
		if ( (ret = copy_to_user(buf, priv->mybuf->reply+priv->offset, copied)) ) {
			priv->offset += copied - ret;
			mycount += copied - ret;
		}
		else if (count < left) {
			priv->offset += copied;
			mycount += copied;
		}
		else {
			mycount += left;
			INIT_LIST_HEAD(&priv->mybuf->list);
			list_add(&priv->mybuf->list,&priv->seeklist);
			priv->mybuf = 0;
			if (priv->seekbufs > 16) {
				struct mybuffers *freebuf = to_mybuffers(priv->seeklist.prev);
				list_del(priv->seeklist.prev);
				kfree(freebuf->buf);
				kfree(freebuf);
			}
			else priv->seekbufs++;
		}
	}

	priv->pos = *offset += mycount;
	up(&priv->sem);

	return mycount;
}


static ssize_t vfi_real_write(struct mybuffers *mybuf, size_t count, loff_t *offset)
{
	int ret;
	int size = 1024-sizeof(struct mybuffers);
	VFI_DEBUG(MY_DEBUG,"%s: count=%d, calls do_operation (...)\n",__FUNCTION__, (int)count);
	ret = do_operation(mybuf->buf, mybuf->reply, &size);

	*offset += count;
	return size;
}

static void queue_to_read(struct privdata *priv, struct mybuffers *mybuf)
{
	VFI_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);
	INIT_LIST_HEAD(&mybuf->list);
	down(&priv->sem);
	if (priv->open) {
		list_add_tail(&mybuf->list,&priv->readlist);
		up(&priv->sem);
		wake_up_interruptible(&priv->rwq);
		return;
	}
	up(&priv->sem);
	kfree(mybuf->buf);
	kfree(mybuf);
}

struct def_work {
	struct mybuffers *mybuf;
	struct privdata *priv;
	size_t count;

	struct work_struct work;
	struct workqueue_struct *woq;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void write_disposeq(void *data)
{
	struct def_work *work = (struct def_work *) data;
#else
static void write_disposeq(struct work_struct *wk)
{
	struct def_work *work = container_of(wk,struct def_work,work);
#endif
	destroy_workqueue(work->woq);
	kfree(work);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void def_write(void *data)
{
	struct def_work *work = (struct def_work *) data;
#else
static void def_write(struct work_struct *wk)
{
	struct def_work *work = container_of(wk,struct def_work,work);
#endif
	loff_t offset = 0;
	int ret;

	ret = vfi_real_write(work->mybuf,work->count,&offset);
		
	work->mybuf->size = ret;
	queue_to_read(work->priv,work->mybuf);
	kobject_put(&work->priv->kobj); 

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	PREPARE_WORK(&work->work, write_disposeq, (void *) work);
#else
	PREPARE_WORK(&work->work, write_disposeq);
#endif
	schedule_work(&work->work);
}

/**
 * vfi_write - Write a command to the vfi driver.
 * @filep - the open filep structure
 * @buf - the user buffer should contain a single, line-feed terminated command
 * @count - the length of the string
 * @offset - the notional offset into the "file".
 *
 * The vfi driver is command string oriented. The char device write
 * command is simply writing a chuck of data in the stream of
 * chararcters. Usually, the glibc/kernel superstructure will cause a
 * line-feed terminated string to flush through the system. However,
 * this is not guaranteed, multiple and/or partial lines may be
 * written. This write routine scans for the first linefeed and writes
 * only the single command. Therefore the API should be careful to
 * flush the writes in the right manner. The synchronous read/write
 * interface is not the prefered interface to the driver, the
 * asynchronous and/or vectored interface of aio_read/aio_write is
 * preferred to avoid these potential problems with a stream vs record
 * interface.
 */
static ssize_t vfi_write(struct file *filep, const char __user *buf, size_t count, loff_t *offset)
{
	struct mybuffers *mybuf;
	char *buffer;
	struct privdata *priv = filep->private_data;
	struct def_work *work;
	size_t thisLen;
	size_t remains;

	VFI_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

	buffer = kzalloc(count+1,GFP_KERNEL);
	if (buffer == NULL)
		return -ENOMEM;

	if ( copy_from_user(buffer,buf,count) ) {
		kfree(buffer);
		return -EFAULT;
	}

	remains = count;

	do {
		mybuf = kzalloc(1024,GFP_KERNEL);
		mybuf->buf = strsep(&buffer,"\n");
		mybuf->reply = (char *)(mybuf+1);

		thisLen =  buffer ? buffer - mybuf->buf : remains;
		remains -= thisLen;

		if (remains) {
			char * remainder = kzalloc(remains+1,GFP_KERNEL);
			if (remainder)
				memcpy(remainder,buffer,remains);
			buffer = remainder;
		}

		if (filep->f_flags & O_NONBLOCK) {
			work = kzalloc(sizeof(struct def_work),GFP_KERNEL);
			if (work == NULL)
				return -ENOMEM;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
			INIT_WORK(&work->work,def_write, (void *) work);
#else
			INIT_WORK(&work->work,def_write);
#endif
			work->woq = create_singlethread_workqueue("vfi_write");
			work->mybuf = mybuf;
			work->count = thisLen;
			work->priv = priv;
			kobject_get(&priv->kobj);
			queue_work(work->woq,&work->work);
		}
		else {
			if (down_interruptible(&priv->sem)) 
				return -ERESTARTSYS;
			mybuf->size = vfi_real_write(mybuf,thisLen,offset);
			up(&priv->sem);
			queue_to_read(priv,mybuf);
		}
	} while (remains && buffer);

	if (filep->f_flags & O_NONBLOCK) {
		if (down_interruptible(&priv->sem)) 
			return -ERESTARTSYS;
		priv->pos = *offset += count;
		up(&priv->sem);
	}

	return count;
}

struct aio_def_work {
	struct kiocb *iocb;
	struct kvec *iovs;
	unsigned long nr_iovs;
	loff_t offset;
	struct work_struct work;
	struct workqueue_struct *woq;
	struct page **uctx;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void aio_disposeq(void *data)
{
	struct aio_def_work *work = (struct aio_def_work *) data;
#else
static void aio_disposeq(struct work_struct *wk)
{
	struct aio_def_work *work = container_of(wk,struct aio_def_work,work);
#endif
	destroy_workqueue(work->woq);
	kfree(work);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void aio_def_write(void *data)
{
	struct aio_def_work *work = (struct aio_def_work *) data;
#else
static void aio_def_write(struct work_struct *wk)
{
	struct aio_def_work *work = container_of(wk,struct aio_def_work,work);
#endif
	struct privdata *priv = work->iocb->ki_filp->private_data;
	int i = 0;
	int numdone = 0;
	int ret = 0;
	ssize_t count = 0;

	VFI_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);
#if 0
	/* Test hack:  Suspend what's on the work queue */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(10000);
#endif
	while (i < work->nr_iovs) {
		if (!ret) {
			struct mybuffers *mybuf = kzalloc(1024,GFP_KERNEL);
			if (mybuf) {
				mybuf->buf = work->iovs[i].iov_base;
				mybuf->reply= (char *)(mybuf+1);

				if ( (ret = vfi_real_write(mybuf, work->iovs[i].iov_len, &work->offset)) < 0 )
					count = ret;
				else 
					count += work->iovs[i].iov_len;

				numdone++;

				if ( ret >= 0 ) {
					mybuf->size = ret;
#ifdef READ_ASYNC_REPLY
					queue_to_read(priv,mybuf);
#else
					/* ki_pos is pointer to user buffer */
	
					if (work->iocb->ki_pos) {
#if 0

 						copy_to_user(work->iocb->ki_pos,
						       	mybuf->reply, 
							strlen(mybuf->reply) + 1);
#endif
						int nr_pages_max;
						int nr_pages;
						int i;
						char *from;
						char *to;
						int ncopy = strlen(mybuf->reply) + 1;
						nr_pages_max = ((PAGE_ALIGN((unsigned long) work->iocb->ki_pos + 256) - ((unsigned long) work->iocb->ki_pos & PAGE_MASK)) >> PAGE_SHIFT);
						nr_pages = ((PAGE_ALIGN((unsigned long) work->iocb->ki_pos + ncopy) - ((unsigned long) work->iocb->ki_pos & PAGE_MASK)) >> PAGE_SHIFT);
						from = mybuf->reply;
						to = kmap(work->uctx[0]);
						to += (work->iocb->ki_pos & ~PAGE_MASK);
						if (nr_pages == 1) {
							memcpy (to, from, ncopy);
							kunmap(work->uctx[0]);
						}
						else {
							int n = PAGE_SIZE - (unsigned long) (from - 
								PAGE_ALIGN((unsigned long) from));
							memcpy (to, from, n);
							kunmap(work->uctx[0]);
							to = kmap(work->uctx[1]);
							from = mybuf->reply + n;
							memcpy (to, from, ncopy - n);
							kunmap(work->uctx[1]);
						}
						for (i = 0; i < nr_pages_max; i++) {
							set_page_dirty_lock(work->uctx[i]);
							page_cache_release(work->uctx[i]);
						}
					}
					kfree(work->uctx);
					kfree(mybuf);
#endif

					ret = 0;
				}
				else {
					kfree(mybuf->buf);
					kfree(mybuf);
				}
			}
			else {
				kfree(work->iovs[i].iov_base);
			}
		}
		else
			kfree(work->iovs[i].iov_base);
		i++;
	}
	kobject_put(&priv->kobj);
	kfree(work->iovs);

#ifdef READ_ASYNC_REPLY
	aio_complete(work->iocb,count,numdone);
#else
	/* return string will be in res2 */
	aio_complete(work->iocb,count,work->iocb->ki_pos);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	PREPARE_WORK(&work->work, aio_disposeq, (void *) work);
#else
	PREPARE_WORK(&work->work, aio_disposeq);
#endif
	schedule_work(&work->work);
}

static ssize_t vfi_aio_write(struct kiocb *iocb, const struct iovec *iovs, unsigned long nr_iovs, loff_t offset)
{
	struct privdata *priv = iocb->ki_filp->private_data;
	struct aio_def_work *work;
	int i = 0;
	int ret = 0;

	VFI_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

	if (is_sync_kiocb(iocb)) {
		return -EIO;
	}

	work = kzalloc(sizeof(struct aio_def_work),GFP_KERNEL);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	INIT_WORK(&work->work,aio_def_write, (void *) work);
#else
	INIT_WORK(&work->work,aio_def_write);
#endif
	work->woq = create_singlethread_workqueue("vfi_aio_write");
	work->iocb = iocb;
	work->iovs = kzalloc(sizeof(struct kvec)*nr_iovs, GFP_KERNEL);
	while ( i < nr_iovs) {
		work->iovs[i].iov_len = iovs[i].iov_len;
		work->iovs[i].iov_base = kzalloc(iovs[i].iov_len+1,GFP_KERNEL);
		if ( (ret = copy_from_user(work->iovs[i].iov_base, iovs[i].iov_base, iovs[i].iov_len)))
			break;
		i++;
	}
	if ( i != nr_iovs) {
		do {
			kfree(work->iovs[i].iov_base);
		} while (i--);
		kfree(work->iovs);
		return ret;
	}

	kobject_get(&priv->kobj);
	work->nr_iovs = nr_iovs;
	work->offset = offset;
#ifndef READ_ASYNC_REPLY   /* pin down response string pages for kernel */
	if (iocb->ki_pos) {
		int nr_pages;
		struct page **uctx;
		nr_pages = ((PAGE_ALIGN((unsigned long) iocb->ki_pos + 256) - 
			((unsigned long) iocb->ki_pos & PAGE_MASK)) >> PAGE_SHIFT);
		uctx = (struct page **) kmalloc(nr_pages * sizeof (struct page *), GFP_KERNEL);
		work->uctx = uctx;

		/* pin pages down */
		down_read(&current->mm->mmap_sem);
		ret = get_user_pages(
			current,
			current->mm,
			(unsigned long) iocb->ki_pos & PAGE_MASK,
			nr_pages,
			1,	/* write */
			0,	/* force */
			uctx,
			NULL);
		up_read(&current->mm->mmap_sem);
		if (ret != nr_pages)
			BUG();
	}
#endif
	queue_work(work->woq,&work->work);
	return -EIOCBQUEUED;
}

#if 0
static int vfi_fsync(struct file *filep, struct dentry *dir, int datasync)
{
	return 0;
}

static int vfi_aio_fsync(struct kiocb *iocb, int datasync)
{
	return 0;
}

static int vfi_fasync(int fd , struct file *filep, int datasync)
{
	return 0;
}
#endif
static unsigned int vfi_poll(struct file *filep, struct poll_table_struct *poll_table)
{
	unsigned int mask = POLLOUT | POLLWRNORM;
	struct privdata *priv = filep->private_data;

	if (down_interruptible(&priv->sem))
		return -ERESTARTSYS;

	poll_wait(filep, &priv->rwq, poll_table);

	if (priv->mybuf || !list_empty(&priv->readlist))
		mask |= POLLIN | POLLRDNORM;

	VFI_DEBUG(MY_DEBUG,"%s mybuf(%p), !list_empty(%d)\n",__FUNCTION__,priv->mybuf,!list_empty(&priv->readlist));
	up(&priv->sem);
	return mask;
}

/**
* vfi_mmap_nopage - lookup page table entry for virtual mapping
* @vma     - virtual memory area descriptor
* @address - virtual address to be mapped
* @type    - type flags
*
* This function is invoked as a "nopage" op by the virtual memory 
* mapping subsystem, typically in response to page faults on virtual
* addresses that have been mmapped onto VFI SMBs. Its job is to 
* return a page table entry that a given virtual address is mapped
* to. 
*
* The @vma structure for VFI mappings will point to private data
* written earlier by vfi_mmap (). That takes the form of an mmap
* ticket that points the page table associated with this area. This
* function uses @address to calculate a page offset within the mapping, 
* and to return a pointer to the page table entry for that page.
*
**/
static struct page* vfi_mmap_nopage (struct vm_area_struct* vma, unsigned long address, int *type)
{
	struct vfi_mmap* tkt = (struct vfi_mmap *)vma->vm_private_data;
	unsigned long pg_off = (address - vma->vm_start) >> PAGE_SHIFT;
#if 0
	VFI_DEBUG (MY_DEBUG, "## vfi_mmap_nopage (%p, %lu, %p)\n", vma, address, type);
	if (tkt) {
		VFI_DEBUG (MY_DEBUG, "-- Original Ticket#%lu, %lu-pages, page table @ %p\n", tkt->t_id, tkt->n_pg, tkt->pg_tbl);
		VFI_DEBUG (MY_DEBUG, "-- Map page %lu of %lu\n", pg_off+1, tkt->n_pg);
	}
	else {
		VFI_DEBUG (MY_DEBUG, "xx Invalid ticket!\n");
	}
#endif
	if (!tkt || pg_off >= tkt->n_pg)
		return NULL;
	else {
		get_page(tkt->pg_tbl[pg_off]);
		return (tkt->pg_tbl[pg_off]);
	}
}

static void vfi_vm_open(struct vm_area_struct *vma)
{
	struct vfi_mmap* tkt = (struct vfi_mmap *)vma->vm_private_data;
	VFI_DEBUG(MY_DEBUG, "%s vma %p mmap %p\n",__func__,vma,tkt);
	vfi_mmap_get(tkt);
}

static void vfi_vm_close(struct vm_area_struct *vma)
{
	struct vfi_mmap* tkt = (struct vfi_mmap *)vma->vm_private_data;
	VFI_DEBUG(MY_DEBUG, "%s vma %p mmap %p\n",__func__,vma,tkt);
	vfi_mmap_put(tkt);
}

static struct vm_operations_struct vm_ops = {
	.open = vfi_vm_open,
	.close = vfi_vm_close,
	.nopage = vfi_mmap_nopage, 
};

/**
* vfi_mmap - map VFI resources into user virtual memory.
*
* @filep: device file pointer
* @vma:   virtual memory area descriptor for pages to be mapped
*
* This function implements the "mmap" service for VFI. Its job
* is to map all of part of a subsidiary VFI memory construct into 
* user virtual address space. The @vma structure describes what is
* to be mapped, and where in virtual memory it is to be mapped to.
*
* Well, sort of.
*
* Hack Attack
* -----------
* VFI does not sit well with the traditional model of device-file
* that mmap expects: it does not possess a unified page table but, 
* rather, manages a disjoint and dynamic collection of local and 
* remote memory objects that each have their own. What that means 
* is that page offsets quoted in mmap calls, which arrive here as
* @vma->vm_pgoff, can not, in fact, be page offsets at all, but 
* must instead be "ticket" identifiers obtained from "smb_mmap" or
* other "_mmap" requests issued to the driver beforehand. Those 
* requests identify page tables and validate mapping requests in
* advance, and leave the results cued in a ticket table that this
* mmap handler can use afterwards. 
*
**/
static int vfi_mmap (struct file* filep, struct vm_area_struct* vma)
{
	struct vfi_mmap *tkt;
	u32 req_size;
	int ret;
	VFI_DEBUG (MY_DEBUG, "** VFI_MMAP *******\n");
	VFI_DEBUG (MY_DEBUG, "Pg: %08lx, Id: %08lx\n", vma->vm_pgoff, (vma->vm_pgoff << PAGE_SHIFT));
	/*
	* Use mmap page offset to locate a ticket created earlier.
	* This ticket tells us what we really need to map to.
	*/
	ret = find_vfi_mmap_by_id(&tkt,vma->vm_pgoff<<PAGE_SHIFT);
	if (!tkt) {
		VFI_DEBUG (MY_DEBUG, "xx Could not locate suitable mmap ticket!\n");
		return -EINVAL;
	}
	
	/*
	* Check that the virtual address range fits inside
	* our target area.
	*/
	req_size = vma->vm_end - vma->vm_start; 
	if ((req_size >> PAGE_SHIFT) > tkt->n_pg) {
		VFI_DEBUG (MY_DEBUG, "xx Required area too big! (%u > %lu)\n", 
			(req_size >> PAGE_SHIFT), tkt->n_pg);
		return (-EINVAL);
	}
	
	if (vma->vm_private_data) {
		VFI_DEBUG (MY_DEBUG, KERN_WARNING "xx Rddma mmap: vma has private data already!\n");
		return (-EINVAL);
	}
	
	/*
	* Copy the ticket into the vma as private data, and
	* erase the original.
	*/
#ifdef FIXED
	vma->vm_private_data = kmalloc (sizeof (struct vfi_mmap_ticket), GFP_KERNEL);
	memcpy (vma->vm_private_data, tkt, sizeof (struct vfi_mmap_ticket));
	vfi_mmap_stamp_ticket (vma->vm_pgoff);
#else
	vma->vm_private_data = (void*)tkt;
#endif
	vma->vm_pgoff = 0;
	vma->vm_ops = &vm_ops;
	tkt = (struct vfi_mmap *)vma->vm_private_data;
	VFI_DEBUG (MY_DEBUG, "-- Set-up ticket for nopage, t_id#%lu, %lu pages, page table @ %p\n", 
		     tkt->t_id, tkt->n_pg, tkt->pg_tbl);
	return 0;
}

static int vfi_open(struct inode *inode, struct file *filep)
{
	struct privdata *priv = kzalloc(sizeof(struct privdata), GFP_KERNEL);
	if ( NULL == priv )
		return -ENOMEM;

	sema_init(&priv->sem,1);
	init_waitqueue_head(&priv->rwq);
	INIT_LIST_HEAD(&priv->readlist);
	INIT_LIST_HEAD(&priv->seeklist);
	kobject_init(&priv->kobj, &privtype);
	priv->open = 1;
	filep->private_data = priv;
	return 0;
}

static int vfi_release(struct inode *inode, struct file *filep)
{
	struct privdata *priv = filep->private_data;
	struct list_head *entry;
	struct list_head *temp;
	if (down_interruptible(&priv->sem))
		return -ERESTARTSYS;

	priv->open = 0;
	list_for_each_safe(entry,temp,&priv->readlist) {
		list_del(entry);
		kfree(to_mybuffers(entry));
	}
	list_for_each_safe(entry,temp,&priv->seeklist) {
		list_del(entry);
		kfree(to_mybuffers(entry));
	}
	priv->seekbufs = 0;
	priv->pos = 0;
	up(&priv->sem);
	kobject_put(&priv->kobj);

	return 0;
}

struct file_operations vfi_file_ops = {
	.owner = THIS_MODULE,
 	.read = vfi_read, 
	.write = vfi_write,
	.llseek = vfi_llseek,
/* 	.fsync = vfi_fsync, */
	.aio_write = vfi_aio_write,
/* 	.aio_fsync = vfi_aio_fsync, */
/* 	.fasync = vfi_fasync, */
	.poll = vfi_poll,
	.mmap = vfi_mmap, 
	.open = vfi_open,
	.release = vfi_release,
};

int vfi_cdev_register(struct vfi_subsys *vfi_dev)
{
	int result;

	if (vfi_major) {
		vfi_dev->dev = MKDEV(vfi_major, vfi_minor);
		result = register_chrdev_region(vfi_dev->dev, vfi_nr_minor, "vfi");
	} else {
		result = alloc_chrdev_region(&vfi_dev->dev, vfi_minor, vfi_nr_minor, "vfi");
		vfi_major = MAJOR(vfi_dev->dev);
	}

	if (result < 0) {
		printk(KERN_WARNING "vfi_cdev_register: can't get major device number %d\n",vfi_major);
		return result;
	}

	cdev_init(&vfi_dev->cdev, &vfi_file_ops);
	vfi_dev->cdev.owner = THIS_MODULE;
	result = cdev_add(&vfi_dev->cdev, vfi_dev->dev, vfi_nr_minor);
	if (result < 0) {
		printk(KERN_WARNING "vfi_cdev_register: cdev_add failed\n");
		goto cdev_fail;
	}

#ifdef CONFIG_PROC_FS
	if (!proc_root_vfi)
		proc_root_vfi = proc_mkdir("vfi", proc_root_driver);
#endif

	return result;

cdev_fail:
	unregister_chrdev_region(vfi_dev->dev, vfi_nr_minor);
	return result;
}

void vfi_cdev_unregister(struct vfi_subsys *vfi_dev)
{
	cdev_del(&vfi_dev->cdev);
	unregister_chrdev_region(vfi_dev->dev, vfi_nr_minor);
#ifdef CONFIG_PROC_FS
	if (proc_root_vfi)
		remove_proc_entry("vfi", proc_root_driver);
	proc_root_vfi = NULL;
#endif
}

EXPORT_SYMBOL(proc_root_vfi);
