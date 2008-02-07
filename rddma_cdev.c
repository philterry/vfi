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

#define MY_DEBUG      RDDMA_DBG_CDEV | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_CDEV | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma.h>

#include <linux/aio.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/proc_fs.h>

#include <linux/rddma_drv.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_mmap.h>
#include <linux/version.h>

struct proc_dir_entry *proc_root_rddma = NULL;

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
	struct list_head list;
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

static ssize_t rddma_read(struct file *filep, char __user *buf, size_t count, loff_t *offset)
{
	int ret;
	int mycount = 0;
	struct privdata *priv = (struct privdata *)filep->private_data;
	int left;

	if (down_interruptible(&priv->sem))
		return -ERESTARTSYS;

	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

	while (!mycount) {
		if (!priv->mybuf) {
			if (!list_empty(&priv->list)) {
				priv->mybuf = to_mybuffers(priv->list.next);
				list_del(priv->list.next);
			}
			while (!priv->mybuf) {
				up(&priv->sem);
				if (filep->f_flags & O_NONBLOCK)
					return -EAGAIN;
				if (wait_event_interruptible(priv->rwq, (priv->mybuf)))
					return -ERESTARTSYS;
				if (down_interruptible(&priv->sem))
					return -ERESTARTSYS;
			}
			priv->size = priv->mybuf->size;
			priv->offset = 0;
		}
		left = priv->size - priv->offset;
		if ( (ret = copy_to_user(buf, priv->mybuf->reply+priv->offset, left)) ) {
			priv->offset += left - ret;
			mycount += left - ret;
			goto out;
		}
		else {
			mycount += left;
			kfree(priv->mybuf->buf);
			kfree(priv->mybuf);
			priv->mybuf = 0;
		}
	}
out:
	up(&priv->sem);

	*offset += mycount;
	return mycount;
}


static ssize_t rddma_real_write(struct mybuffers *mybuf, size_t count, loff_t *offset)
{
	int ret;
	RDDMA_DEBUG(MY_DEBUG,"%s: count=%d, calls do_operation (...)\n",__FUNCTION__, (int)count);
	ret = do_operation(mybuf->buf, mybuf->reply, 1024-sizeof(struct mybuffers));

	if ( ret < 0 ) {
		return ret;
	}

	*offset += count;
	return ret;
}

static void queue_to_read(struct privdata *priv, struct mybuffers *mybuf)
{
	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);
	INIT_LIST_HEAD(&mybuf->list);
	down(&priv->sem);
	if (priv->open) {
		list_add_tail(&mybuf->list,&priv->list);
		up(&priv->sem);
		wake_up_interruptible(&priv->rwq);
		return;
	}
	up(&priv->sem);
	kfree(mybuf->buf);
	kfree(mybuf);
}

/**
 * rddma_write - Write a command to the rddma driver.
 * @filep - the open filep structure
 * @buf - the user buffer should contain a single, line-feed terminated command
 * @count - the length of the string
 * @offset - the notional offset into the "file".
 *
 * The rddma driver is command string oriented. The char device write
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
static ssize_t rddma_write(struct file *filep, const char __user *buf, size_t count, loff_t *offset)
{
	int ret;
	struct mybuffers *mybuf;
	char *buffer = kzalloc(count+1,GFP_KERNEL);
	struct privdata *priv = filep->private_data;

	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);
	if ( (ret = copy_from_user(buffer,buf,count)) ) {
		kfree(buffer);
		return -EFAULT;
	}

	mybuf = kzalloc(1024,GFP_KERNEL);
	mybuf->buf = strsep(&buffer,"\n");
	mybuf->reply = (char *)(mybuf+1);

	ret = rddma_real_write(mybuf,count,offset);
	
	if ( ret < 0 ) {
		kfree(mybuf->buf);
		kfree(mybuf);
		return ret;
	}
	
	mybuf->size = ret;
	queue_to_read(priv,mybuf);

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

	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);
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

				if ( (ret = rddma_real_write(mybuf, work->iovs[i].iov_len, &work->offset)) < 0 )
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

	destroy_workqueue(work->woq);
	kfree(work);
}

static ssize_t rddma_aio_write(struct kiocb *iocb, const struct iovec *iovs, unsigned long nr_iovs, loff_t offset)
{
	struct privdata *priv = iocb->ki_filp->private_data;
	char name[10];
	struct aio_def_work *work;
	int i = 0;
	int ret = 0;

	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

	if (is_sync_kiocb(iocb)) {
		return -EIO;
	}

	work = kzalloc(sizeof(struct aio_def_work),GFP_KERNEL);
	sprintf(name,"%p",work);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	INIT_WORK(&work->work,aio_def_write, (void *) work);
#else
	INIT_WORK(&work->work,aio_def_write);
	work->woq = create_singlethread_workqueue(name);
#endif
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
static int rddma_fsync(struct file *filep, struct dentry *dir, int datasync)
{
	return 0;
}

static int rddma_aio_fsync(struct kiocb *iocb, int datasync)
{
	return 0;
}

static int rddma_fasync(int fd , struct file *filep, int datasync)
{
	return 0;
}
#endif
static unsigned int rddma_poll(struct file *filep, struct poll_table_struct *poll_table)
{
	unsigned int mask = POLLOUT | POLLWRNORM;
	struct privdata *priv = filep->private_data;

	if (down_interruptible(&priv->sem))
		return -ERESTARTSYS;

	poll_wait(filep, &priv->rwq, poll_table);

	if (priv->mybuf)
		return mask |= POLLIN | POLLRDNORM;

	up(&priv->sem);

	return mask;
}

/**
* rddma_mmap_nopage - lookup page table entry for virtual mapping
* @vma     - virtual memory area descriptor
* @address - virtual address to be mapped
* @type    - type flags
*
* This function is invoked as a "nopage" op by the virtual memory 
* mapping subsystem, typically in response to page faults on virtual
* addresses that have been mmapped onto RDDMA SMBs. Its job is to 
* return a page table entry that a given virtual address is mapped
* to. 
*
* The @vma structure for RDDMA mappings will point to private data
* written earlier by rddma_mmap (). That takes the form of an mmap
* ticket that points the page table associated with this area. This
* function uses @address to calculate a page offset within the mapping, 
* and to return a pointer to the page table entry for that page.
*
**/
static struct page* rddma_mmap_nopage (struct vm_area_struct* vma, unsigned long address, int *type)
{
	struct rddma_mmap* tkt = (struct rddma_mmap *)vma->vm_private_data;
	unsigned long pg_off = (address - vma->vm_start) >> PAGE_SHIFT;
#if 0
	RDDMA_DEBUG (MY_DEBUG, "## rddma_mmap_nopage (%p, %lu, %p)\n", vma, address, type);
	if (tkt) {
		RDDMA_DEBUG (MY_DEBUG, "-- Original Ticket#%lu, %lu-pages, page table @ %p\n", tkt->t_id, tkt->n_pg, tkt->pg_tbl);
		RDDMA_DEBUG (MY_DEBUG, "-- Map page %lu of %lu\n", pg_off+1, tkt->n_pg);
	}
	else {
		RDDMA_DEBUG (MY_DEBUG, "xx Invalid ticket!\n");
	}
#endif
	return ((!tkt || pg_off >= tkt->n_pg) ? NULL : tkt->pg_tbl[pg_off]);
	
}

static struct vm_operations_struct vm_ops = {
	.nopage = rddma_mmap_nopage, 
};

/**
* rddma_mmap - map RDDMA resources into user virtual memory.
*
* @filep: device file pointer
* @vma:   virtual memory area descriptor for pages to be mapped
*
* This function implements the "mmap" service for RDDMA. Its job
* is to map all of part of a subsidiary RDDMA memory construct into 
* user virtual address space. The @vma structure describes what is
* to be mapped, and where in virtual memory it is to be mapped to.
*
* Well, sort of.
*
* Hack Attack
* -----------
* RDDMA does not sit well with the traditional model of device-file
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
static int rddma_mmap (struct file* filep, struct vm_area_struct* vma)
{
	struct rddma_mmap *tkt;
	u32 req_size;
	RDDMA_DEBUG (MY_DEBUG, "** RDDMA_MMAP *******\n");
	RDDMA_DEBUG (MY_DEBUG, "Pg: %08lx, Id: %08lx\n", vma->vm_pgoff, (vma->vm_pgoff << PAGE_SHIFT));
	/*
	* Use mmap page offset to locate a ticket created earlier.
	* This ticket tells us what we really need to map to.
	*/
	tkt = find_rddma_mmap_by_id(vma->vm_pgoff<<PAGE_SHIFT);
	if (!tkt) {
		RDDMA_DEBUG (MY_DEBUG, "xx Could not locate suitable mmap ticket!\n");
		return -EINVAL;
	}
	
	/*
	* Check that the virtual address range fits inside
	* our target area.
	*/
	req_size = vma->vm_end - vma->vm_start; 
	if ((req_size >> PAGE_SHIFT) > tkt->n_pg) {
		RDDMA_DEBUG (MY_DEBUG, "xx Required area too big! (%u > %lu)\n", 
			(req_size >> PAGE_SHIFT), tkt->n_pg);
		return (-EINVAL);
	}
	
	if (vma->vm_private_data) {
		RDDMA_DEBUG (MY_DEBUG, KERN_WARNING "xx Rddma mmap: vma has private data already!\n");
		return (-EINVAL);
	}
	
	/*
	* Copy the ticket into the vma as private data, and
	* erase the original.
	*/
#ifdef FIXED
	vma->vm_private_data = kmalloc (sizeof (struct rddma_mmap_ticket), GFP_KERNEL);
	memcpy (vma->vm_private_data, tkt, sizeof (struct rddma_mmap_ticket));
	rddma_mmap_stamp_ticket (vma->vm_pgoff);
#else
	vma->vm_private_data = (void*)tkt;
#endif
	vma->vm_pgoff = 0;
	vma->vm_ops = &vm_ops;
	tkt = (struct rddma_mmap *)vma->vm_private_data;
	RDDMA_DEBUG (MY_DEBUG, "-- Set-up ticket for nopage, t_id#%lu, %lu pages, page table @ %p\n", 
		     tkt->t_id, tkt->n_pg, tkt->pg_tbl);
	return 0;
}

static int rddma_open(struct inode *inode, struct file *filep)
{
	struct privdata *priv = kzalloc(sizeof(struct privdata), GFP_KERNEL);
	if ( NULL == priv )
		return -ENOMEM;

	sema_init(&priv->sem,1);
	init_waitqueue_head(&priv->rwq);
	INIT_LIST_HEAD(&priv->list);
	priv->kobj.ktype = &privtype;
	kobject_init(&priv->kobj);
	priv->open = 1;
	filep->private_data = priv;
	return 0;
}

static int rddma_release(struct inode *inode, struct file *filep)
{
	struct privdata *priv = filep->private_data;
	struct list_head *entry;
	struct list_head *temp;
	if (down_interruptible(&priv->sem))
		return -ERESTARTSYS;

	priv->open = 0;
	list_for_each_safe(entry,temp,&priv->list) {
		list_del(entry);
		kfree(to_mybuffers(entry));
	}
	up(&priv->sem);
	kobject_put(&priv->kobj);

	return 0;
}

struct file_operations rddma_file_ops = {
	.owner = THIS_MODULE,
 	.read = rddma_read, 
	.write = rddma_write,
/* 	.fsync = rddma_fsync, */
	.aio_write = rddma_aio_write,
/* 	.aio_fsync = rddma_aio_fsync, */
/* 	.fasync = rddma_fasync, */
	.poll = rddma_poll,
	.mmap = rddma_mmap, 
	.open = rddma_open,
	.release = rddma_release,
};

int rddma_cdev_register(struct rddma_subsys *rddma_dev)
{
	int result;

	if (rddma_major) {
		rddma_dev->dev = MKDEV(rddma_major, rddma_minor);
		result = register_chrdev_region(rddma_dev->dev, rddma_nr_minor, "rddma");
	} else {
		result = alloc_chrdev_region(&rddma_dev->dev, rddma_minor, rddma_nr_minor, "rddma");
		rddma_major = MAJOR(rddma_dev->dev);
	}

	if (result < 0) {
		printk(KERN_WARNING "rddma_cdev_register: can't get major device number %d\n",rddma_major);
		return result;
	}

	cdev_init(&rddma_dev->cdev, &rddma_file_ops);
	rddma_dev->cdev.owner = THIS_MODULE;
	result = cdev_add(&rddma_dev->cdev, rddma_dev->dev, rddma_nr_minor);
	if (result < 0) {
		printk(KERN_WARNING "rddma_cdev_register: cdev_add failed\n");
		goto cdev_fail;
	}

#ifdef CONFIG_PROC_FS
	if (!proc_root_rddma)
		proc_root_rddma = proc_mkdir("rddma", proc_root_driver);
#endif

	return result;

cdev_fail:
	unregister_chrdev_region(rddma_dev->dev, rddma_nr_minor);
	return result;
}

void rddma_cdev_unregister(struct rddma_subsys *rddma_dev)
{
	cdev_del(&rddma_dev->cdev);
	unregister_chrdev_region(rddma_dev->dev, rddma_nr_minor);
}

EXPORT_SYMBOL(proc_root_rddma);
