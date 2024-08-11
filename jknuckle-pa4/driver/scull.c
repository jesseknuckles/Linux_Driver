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
#include <linux/mutex.h>  // for mutex

#include <linux/uaccess.h>	/* copy_*_user */

#include "scull.h"		/* local definitions */

#include <linux/sched.h>
#include <linux/smp.h>

/*
 * Our parameters which can be set at load time.
 */

//pthread_mutex_t mutex;
static int scull_major =   SCULL_MAJOR;
static int scull_minor =   0;
static int scull_quantum = SCULL_QUANTUM;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);

MODULE_AUTHOR("jknuckle"); //my uname
MODULE_LICENSE("Dual BSD/GPL");

//MY SHIZ

//init mutex
static DEFINE_MUTEX(mux); 

// getting task struct
task_info tinfo;
task_info user_tinfo;

//sets up the task struct with the proper values
void init_task_info(task_info* tinfo) {
	tinfo->__state = current->__state;
	tinfo->cpu = smp_processor_id();
	tinfo->prio = current->prio;
	tinfo->pid = current->pid;
	tinfo->tgid = current->tgid;
	tinfo->nvcsw = current->nvcsw;
	tinfo->nivcsw = current->nivcsw;
}

//adding node to LL
void add_node(task_info tinfo, LL* pll, int* retval) {
	int j = tinfo.pid; //for checking for duplicates
	node* temp; //for traversing through the linked list
	node* insert = (node*) kmalloc(sizeof(node), GFP_KERNEL); //node ptr that will actually be inserted to ll
	if (insert == NULL) { // checks kmalloc error
		*retval = -1;
	}
	insert->pid = tinfo.pid; // fills node pointed to by insert with proper values
	insert->tgid = tinfo.tgid;
	insert->next = NULL;
	temp = pll->head; // start ll traversal
	if (temp == NULL) { //insert insert as head if head is null
		pll->head = insert;
		return;
	}
	else { // if head isn't null
		while (temp->next != NULL) {
			if (temp->pid == j) { // if curr process's pid is a duplicate
				kfree(insert); // gotta free insert sisnce it won't be inserted into ll
				return;
			}
			temp = temp->next; // traverse through the list
		}
		if (temp->pid == j) { // free insert if it is a duplicate pid
			kfree(insert);
			return;
		}
		temp->next = insert; //place insert into the ll appropriately
		return;
	}
}

//destory LL
void destroy_LL(LL* pll) { //frees allocated space
	node* temp = pll->head; //to traverse through the list
	node* tempd; //to free after going to temp-> next
	while (temp != NULL) { // while loop
		tempd = temp;
		temp = temp->next;
		kfree(tempd); //free node
	}
	kfree(pll); //free tree
}

//print LL
void print_ll(LL* pll) {
	int i = 0; //keep track of task #
	node* temp = pll->head; //to traverse ll
	if (temp == NULL) { // if head is null there is nothing to print
		return;
	}
	else { //if head is not null
		while (temp != NULL) { //print the info of the nodes until it is null
			printk("Task %d: PID %d, TGID %d\n", i+1, temp->pid, temp->tgid);
			temp = temp->next;
			i++;
		}
		return;
	}
}

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

/*
 * The ioctl() implementation
 */

static long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{	
	int err = 0, tmp;
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

	case SCULL_IOCRESET:
		scull_quantum = SCULL_QUANTUM;
		break;
        
	case SCULL_IOCSQUANTUM: /* Set: arg points to the value */
		retval = __get_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCTQUANTUM: /* Tell: arg is the value */
		scull_quantum = arg;
		break;

	case SCULL_IOCGQUANTUM: /* Get: arg is pointer to result */
		retval = __put_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCQQUANTUM: /* Query: return it (it's positive) */
		return scull_quantum;

	case SCULL_IOCXQUANTUM: /* eXchange: use arg as pointer */
		tmp = scull_quantum;
		retval = __get_user(scull_quantum, (int __user *)arg);
		if (retval == 0)
			retval = __put_user(tmp, (int __user *)arg);
		break;

	case SCULL_IOCHQUANTUM: /* sHift: like Tell + Query */
		tmp = scull_quantum;
		scull_quantum = arg;
		return tmp;

	case SCULL_IOCIQUANTUM: // case for when SCULL_IOCIQUANTUM is called.
		mutex_lock(&mux); //manage concurrency
		init_task_info(&tinfo); //fill info_struct with values
		if (copy_to_user((task_info __user *)arg, &tinfo, sizeof(tinfo)) != 0) { //check for error
			retval = -1;
		}
		add_node(tinfo, pll, &retval); //add to ll
		mutex_unlock(&mux); //unlock
		break;

	default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;
}

struct file_operations scull_fops = {
	.owner =    THIS_MODULE,
	.unlocked_ioctl = scull_ioctl,
	.open =     scull_open,
	.release =  scull_release,
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

	/* Get rid of the char dev entry */
	cdev_del(&scull_cdev);

	//destroy pll
	mutex_lock(&mux); //lock for printing and destroying ll
	print_ll(pll); //print ll
	destroy_LL(pll); //destory ll
	mutex_unlock(&mux); //unlock

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);
}

int scull_init_module(void)
{
	int result;
	dev_t dev = 0;

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
	//initialize linked list
	pll = (LL*)kmalloc(sizeof(LL*), GFP_KERNEL); //allocate ll on startup
	if (pll == NULL) { //check kmalloc error
		goto fail;
	}
	pll->head = NULL; //set head to NULL
	

	return 0; /* succeed */

  fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
