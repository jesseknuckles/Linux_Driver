This repository conains a Linux Driver in the form of a kernel module written in c. I programmed a new IOCTL command in the driver to interact with a kernel task_struct inside the 
Linux kernel. I created a special character device file and associated it with my driver by observing the major number of my custom driver. I wrote a user space c program to create 
processes and threads to interact with the special char file via my driver to retrieve kernel information about the process or thread. I handled concurrency appropriately. This 
project expanded my knowledge of how dirvers interact with physical components of a computer when a program calls an I/O operation concerning this hardware. This project also helped 
me learn how to install and uninstall kernel modules, and check the kernel log to verify the installations are occuring.
