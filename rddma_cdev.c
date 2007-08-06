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

#include <linux/aio.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/sched.h>

#include <linux/rddma_drv.h>
#include <linux/rddma_ops.h>
#include <linux/version.h>

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

	RDDMA_DEBUG(1,"%s entered\n",__FUNCTION__);

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

static ssize_t rddma_aio_read(struct kiocb * iocb, const struct iovec *iovs, unsigned long nr_iovs, loff_t offset)
{
	if (is_sync_kiocb(iocb)) {
		int i = 0;
		ssize_t count = 0;
		while (i < nr_iovs) {
			count += rddma_read(iocb->ki_filp, iovs[i].iov_base, iovs[i].iov_len, &offset);
			i++;
		}
		return count;
	}
	return 0;
}

static ssize_t rddma_real_write(struct mybuffers *mybuf, size_t count, loff_t *offset)
{
	int ret;
	RDDMA_DEBUG(1,"%s entered count=%d\n",__FUNCTION__, count);
	ret = do_operation(mybuf->buf, mybuf->reply, 1024-sizeof(struct mybuffers));

	if ( ret < 0 ) {
		return ret;
	}

	*offset += count;
	return ret;
}

static void queue_to_read(struct privdata *priv, struct mybuffers *mybuf)
{
	RDDMA_DEBUG(1,"%s entered\n",__FUNCTION__);
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

	RDDMA_DEBUG(1,"%s entered\n",__FUNCTION__);
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
					queue_to_read(priv,mybuf);
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
	aio_complete(work->iocb,count,numdone);
	kfree(work);
}

static ssize_t rddma_aio_write(struct kiocb *iocb, const struct iovec *iovs, unsigned long nr_iovs, loff_t offset)
{
	struct privdata *priv = iocb->ki_filp->private_data;

	if (is_sync_kiocb(iocb)) {
		int i = 0;
		int ret = 0;
		ssize_t count = 0;
		while (i < nr_iovs) {
			struct mybuffers *mybuf;
			char *buffer = kzalloc(iovs[i].iov_len+1, GFP_KERNEL);
			if (!buffer) 
				return -ENOMEM;

			if (copy_from_user(buffer, iovs[i].iov_base,iovs[i].iov_len)) {
				kfree(buffer);
				return -EFAULT;
			}
			
			if (!(mybuf = kzalloc(1024,GFP_KERNEL))) {
				kfree(buffer);
				return -ENOMEM;
			}

			mybuf->buf = buffer;
			mybuf->reply= (char *)(mybuf+1);

			ret = rddma_real_write(mybuf, iovs[i].iov_len, &offset);
			
			if (  ret < 0 ) {
				kfree(mybuf->buf);
				kfree(mybuf);
				return ret;
			}

			mybuf->size = ret;
			queue_to_read(priv,mybuf);

			count += iovs[i].iov_len;
			i++;
		}
		return count;
	}
	else {
		struct aio_def_work *work = kzalloc(sizeof(struct aio_def_work),GFP_KERNEL);
		int i = 0;
		int ret = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
		INIT_WORK(&work->work,aio_def_write, (void *) work);
#else
		INIT_WORK(&work->work,aio_def_write);
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
		schedule_work(&work->work);
		return -EIOCBQUEUED;
	}
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
 	.aio_read = rddma_aio_read, 
	.aio_write = rddma_aio_write,
/* 	.aio_fsync = rddma_aio_fsync, */
/* 	.fasync = rddma_fasync, */
	.poll = rddma_poll,
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
