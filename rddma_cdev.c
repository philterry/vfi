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
#include <linux/spinlock.h>

#include <linux/rddma_drv.h>
#include <linux/rddma_ops.h>

struct mybuffers {
	struct list_head list;
	int size;
	char *buf;
};

struct privdata {
	spinlock_t lock;
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

	while (mycount < count) {
		if (!priv->mybuf) {
			spin_lock(&priv->lock);
			if (!list_empty(&priv->list)) {
				priv->mybuf = list_first_entry(&priv->list, struct mybuffers, list);
				list_del(&priv->mybuf->list);
			}
			spin_unlock(&priv->lock);
			if (!priv->mybuf)
				goto out;
			priv->size = priv->mybuf->size;
			priv->offset = 0;
		}
		left = priv->size - priv->offset;
		if ( (ret = copy_to_user(buf, priv->mybuf->buf+priv->offset, left)) ) {
			priv->offset += left - ret;
			mycount += left - ret;
			goto out;
		}
		else {
			mycount += left;
			kfree(priv->mybuf);
			priv->mybuf = 0;
		}
	}
out:
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
	return 0;;
}

#define BUF_EXTEND(size) ((size)+sizeof(struct mybuffers)+((size)>>1)+2)

static ssize_t rddma_real_write(char *buf, size_t count, loff_t *offset)
{
	int ret;
	
	ret = do_operation(buf, buf+count+1, count);

	if (strstr(buf,"?"))
		buf[count] = ',';
	else
		buf[count] = '?';
	
	if ( ret ) {
		return ret;
	}

	*offset += count;
	return count;
}

static void queue_to_read(struct privdata *priv, struct mybuffers *mybuf)
{
	INIT_LIST_HEAD(&mybuf->list);
	spin_lock(&priv->lock);
	if (priv->open) {
		list_add_tail(&priv->list,&mybuf->list);
		spin_unlock(&priv->lock);
		return;
	}
	spin_unlock(&priv->lock);
	kfree(mybuf->buf);
}

static ssize_t rddma_write(struct file *filep, const char __user *buf, size_t count, loff_t *offset)
{
	int ret;
	char *buffer = kzalloc(BUF_EXTEND(count),GFP_KERNEL);
	struct mybuffers *mybuf = (struct mybuffers *)buffer+((count+1)<<1);
	struct privdata *priv = filep->private_data;

	if ( (ret = copy_from_user(buffer,buf,count)) ) {
		kfree(buffer);
		return ret;
	}

	ret = rddma_real_write(buffer,count,offset);

	mybuf->buf=buffer;
	queue_to_read(priv,mybuf);

	return ret;
}

struct aio_def_work {
	struct kiocb *iocb;
	struct kvec *iovs;
	unsigned long nr_iovs;
	loff_t offset;
	struct work_struct work;
};

static void aio_def_write(struct work_struct *wk)
{
	struct aio_def_work *work = container_of(wk,struct aio_def_work,work);
	struct privdata *priv = work->iocb->ki_filp->private_data;
	int i = 0;
	int numdone = 0;
	int ret = 0;
	ssize_t count = 0;
	while (i < work->nr_iovs) {
		if (!ret) {
			struct mybuffers *mybuf = (struct mybuffers *)work->iovs[i].iov_base+((work->iovs[i].iov_len+1)<<1);
			if ( (ret = rddma_real_write(work->iovs[i].iov_base, work->iovs[i].iov_len, &work->offset)) < 0 )
				count = ret;
			else {
				count += ret;
				ret = 0;
			}
			numdone++;
			mybuf->buf=work->iovs[i].iov_base;
			queue_to_read(priv,mybuf);
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
			char *buffer = kzalloc(BUF_EXTEND(iovs[i].iov_len), GFP_KERNEL);
			struct mybuffers *mybuf = (struct mybuffers *)buffer+((iovs[i].iov_len+1)<<1);
			if (copy_from_user(buffer, iovs[i].iov_base,iovs[i].iov_len)) {
				kfree(buffer);
				return -EFAULT;
			}
			ret = rddma_real_write(buffer, iovs[i].iov_len, &offset);
			
			mybuf->buf = buffer;
			queue_to_read(priv,mybuf);

			if (  ret < 0 ) {
				return ret;
			}
			count += ret;
			i++;
		}
		return count;
	}
	else {
		struct aio_def_work *work = kzalloc(sizeof(struct aio_def_work),GFP_KERNEL);
		int i = 0;
		int ret = 0;
		INIT_WORK(&work->work,aio_def_write);
		work->iocb = iocb;
		work->iovs = kzalloc(sizeof(struct kvec)*nr_iovs, GFP_KERNEL);
		while ( i < nr_iovs) {
			work->iovs[i].iov_len = iovs[i].iov_len;
			work->iovs[i].iov_base = kzalloc(BUF_EXTEND(iovs[i].iov_len),GFP_KERNEL);
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
	int ret = -ENOMEM;
	struct privdata *priv = kzalloc(sizeof(struct privdata), GFP_KERNEL);
	if ( NULL == priv )
		goto fail;

	spin_lock_init(&priv->lock);
	INIT_LIST_HEAD(&priv->list);
	priv->kobj.ktype = &privtype;
	kobject_init(&priv->kobj);
	priv->open = 1;
	filep->private_data = priv;
	ret = 0;
fail:
	return ret;
}

static int rddma_release(struct inode *inode, struct file *filep)
{
	struct privdata *priv = filep->private_data;
	struct list_head *entry;
	spin_lock(&priv->lock);
	priv->open = 0;
	list_for_each(entry,&priv->list) {
		list_del(entry);
		kfree(entry);
	}
	spin_unlock(&priv->lock);
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
