This repository contains two Linux drivers in the form of kernel modules written in c. 

In the driver under pa4, I programmed a new IOCTL command to interact with a kernel task_struct inside the Linux kernel. I created a special character device file and associated it with my driver by observing the major number of my custom driver. I wrote a user space c program to create processes and threads to interact with the special char file via my driver to retrieve kernel information about the process or thread. I handled concurrency appropriately.

In the driver under pa5, I programmed several new IOCTL commands to interact with our special character device file. Within the kernel, I programmed a linked list that stores stores a character as each node. Then, according to the inputs of the user space ioctl command, a string would safely be copied from user space to the linked list in kernel space. Then, this linked list would read/write to the special character file, byte by byte, according to the inputs of the ioctl command.

These projects expanded my knowledge of how drivers interact with physical components of a computer when a program calls an I/O operation concerning this hardware. These projects also helped me learn how to install and uninstall kernel modules, and check the kernel log to verify the installations are occuring.
