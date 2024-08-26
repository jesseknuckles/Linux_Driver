/*
 * scull.c -- the bare scull char module
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/cdev.h>
#include <linux/mutex.h> //for the mutex
#include <linux/semaphore.h> //for the semaphore
#include <linux/errno.h> //for the error returns


#include <linux/uaccess.h>	/* copy_*_user */

#include "scull.h"		/* local definitions */

/*
 * Our parameters which can be set at load time.
 */

static int scull_major =   SCULL_MAJOR;
static int scull_minor =   0;
static int scull_fifo_elemsz = SCULL_FIFO_ELEMSZ_DEFAULT; /* ELEMSZ */
static int scull_fifo_size   = SCULL_FIFO_SIZE_DEFAULT;   /* N      */

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_fifo_size, int, S_IRUGO);
module_param(scull_fifo_elemsz, int, S_IRUGO);

MODULE_AUTHOR("jknuckle");
MODULE_LICENSE("Dual BSD/GPL");

static struct cdev scull_cdev;		/* Char device structure */

/*
 * Open and close
 */

static int scull_open(struct inode *inode, struct file *filp)
{	
	printk(KERN_INFO "scull open\n");
	return 0;          /* success */
}

static int scull_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "scull close\n");
	return 0;
}

//define mutex
static DEFINE_MUTEX(mux);
struct semaphore writee; //semaphore that blocks writing to queue
struct semaphore reade; //semaphore thay blocks reading from queue

/*
 * Read and Write
 */
static ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	/* TODO: implement this function */
	
	if(down_interruptible(&reade) != 0) { //access queue only if non-empty
		//return this if interupted
		return -ERESTARTSYS;
	}
	if (mutex_lock_interruptible(&mux)!= 0) { //only one process can change queue to avoid race condition
		//return this if interrupted
		return -ERESTARTSYS;
	}
	printk(KERN_INFO "scull read\n");

	if (*((size_t*) mqueueo) < count) {
		count = *((size_t*) mqueueo); // adjust value of count if it is larger than len of next elem
	} 
	mqueueo = mqueueo + sizeof(size_t); //go to start of message in queue

	if(copy_to_user(buf, mqueueo, count)) {
		return -EFAULT; // return this if copy from queue to user space is unsuccessful.
	}

	if (((char*)mqueueo) > (start + ((scull_fifo_size-1) * (sizeof(size_t)+scull_fifo_elemsz)))) {
		mqueueo = start; // go to start if at the end of the queue
	}
	else {
		mqueueo = mqueueo + scull_fifo_elemsz; // go to next element in queue
	}
	mutex_unlock(&mux); //unlock mutex
	up(&writee); //signify to write that there is one less space in the buffer
	return count; //return count on success.
}


static ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	/* TODO: implement this function */
	if(down_interruptible(&writee) != 0) { //access if queue isn't full
		//return this if interupted.
		return -ERESTARTSYS;
	}
	if (mutex_lock_interruptible(&mux)!= 0) { //avoids race conditions
		//return this if interupted.
		return -ERESTARTSYS;
	}
	printk(KERN_INFO "scull write\n");

	if (scull_fifo_elemsz < count) {
		count = scull_fifo_elemsz; // adjust value of count if its larger than mex len allowed for message
	} 
	*((size_t*)mqueuei) = count; //add length of next elem to the queue
	mqueuei = mqueuei + sizeof(size_t); //go to start point of message

	if (copy_from_user(mqueuei, buf, count) != 0) {
		return -EFAULT; //return this if copy from user didn't work properly
	}

	if (((char*)mqueuei) > (start + ((scull_fifo_size-1) * (sizeof(size_t)+scull_fifo_elemsz)))) {
		mqueuei = start; //wrap around the queue if at the last element of queue
	}

	else {
		mqueuei = mqueuei + scull_fifo_elemsz; // go to where len of next message will be written in queue
	}
	mutex_unlock(&mux); //unlock mutex
	up(&reade); //tell read that queue added a message
	return count;
}

/*
 * The ioctl() implementation
 */
static long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0;
	int retval = 0;
    
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

	err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {
	case SCULL_IOCGETELEMSZ:
		return scull_fifo_elemsz;

	default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;

}

struct file_operations scull_fops = {
	.owner 		= THIS_MODULE,
	.unlocked_ioctl = scull_ioctl,
	.open 		= scull_open,
	.release	= scull_release,
	.read 		= scull_read,
	.write 		= scull_write,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void scull_cleanup_module(void)
{
	dev_t devno = MKDEV(scull_major, scull_minor);

	/* TODO: free FIFO safely here */

	/* Get rid of the char dev entry */
	cdev_del(&scull_cdev);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);
	kfree(start); //free queue

}

int scull_init_module(void)
{
	int result;
	dev_t dev = 0;


	//initiaize the message queue
	start = (char*) kmalloc(scull_fifo_size * (sizeof(size_t)+scull_fifo_elemsz), GFP_KERNEL);
	if (start == NULL) { //return on error
		return -ENOMEM;
	}
	mqueueo = start; //make two void pointers, one for where new message will be added to queue
	mqueuei = start; //and one where next message will be read from queue

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, 1, "scull");
	} else {
		result = alloc_chrdev_region(&dev, scull_minor, 1, "scull");
		scull_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	cdev_init(&scull_cdev, &scull_fops);
	scull_cdev.owner = THIS_MODULE;
	result = cdev_add (&scull_cdev, dev, 1);
	/* Fail gracefully if need be */
	if (result) {
		printk(KERN_NOTICE "Error %d adding scull character device", result);
		goto fail;
	}

	/* TODO: allocate FIFO correctly here */

	printk(KERN_INFO "scull: FIFO SIZE=%u, ELEMSZ=%u\n", scull_fifo_size, scull_fifo_elemsz);

	//initialize semaphores.
	sema_init(&reade, 0);
	sema_init(&writee, scull_fifo_size);

	return 0; /* succeed */

  fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
