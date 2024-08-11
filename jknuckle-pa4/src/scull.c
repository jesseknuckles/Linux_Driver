#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <time.h>


#include "scull.h"

#define CDEV_NAME "/dev/scull"

/* Quantum command line option */
static int g_quantum;

//my function that prints the output for the "i" argument
void print_task_info(task_info tinfo) {
	printf("state %d, cpu %d, prio %d, pid %d, tgid %d, nv %ld, niv %ld\n", tinfo.__state, tinfo.cpu, tinfo.prio, tinfo.pid, tinfo.tgid, tinfo.nvcsw, tinfo.nivcsw);
}

// function for my threads
void* t_function(void* fd) {
	task_info tinfot; //make task_info local variable to avoid need for user space mutexes
	int* use = (int*) fd; //to avoid dereferencing a void*

	//to calls to driver fucntion and printing the task_info
	ioctl(*use, SCULL_IOCIQUANTUM, &tinfot);
	print_task_info(tinfot);
	
	ioctl(*use, SCULL_IOCIQUANTUM, &tinfot);
	print_task_info(tinfot);
	
	//exit Pthread
	pthread_exit(NULL);
}

static void usage(const char *cmd)
{
	printf("Usage: %s <command>\n"
	       "Commands:\n"
	       "  R          Reset quantum\n"
	       "  S <int>    Set quantum\n"
	       "  T <int>    Tell quantum\n"
	       "  G          Get quantum\n"
	       "  Q          Query quantum\n"
	       "  X <int>    Exchange quantum\n"
	       "  H <int>    Shift quantum\n"
	       "  h          Print this message\n",
	       cmd);
}

typedef int cmd_t;

static cmd_t parse_arguments(int argc, const char **argv)
{
	cmd_t cmd;

	if (argc < 2) {
		fprintf(stderr, "%s: Invalid number of arguments\n", argv[0]);
		cmd = -1;
		goto ret;
	}

	/* Parse command and optional int argument */
	cmd = argv[1][0];
	switch (cmd) {
	case 'S':
	case 'T':
	case 'H':
	case 'X':
		if (argc < 3) {
			fprintf(stderr, "%s: Missing quantum\n", argv[0]);
			cmd = -1;
			break;
		}
		g_quantum = atoi(argv[2]);
		break;
	case 'R':
	case 'G':
	case 'Q':
	case 'h':
	case 'i': //include this and two lines below so that invalid command message won't be shown
	case 'p':
	case 't':
		break;
	default:
		fprintf(stderr, "%s: Invalid command\n", argv[0]);
		cmd = -1;
	}

ret:
	if (cmd < 0 || cmd == 'h') {
		usage(argv[0]);
		exit((cmd == 'h')? EXIT_SUCCESS : EXIT_FAILURE);
	}
	return cmd;
}

static int do_op(int fd, cmd_t cmd)
{
	int ret, q;

	switch (cmd) {
	case 'R':
		ret = ioctl(fd, SCULL_IOCRESET);
		if (ret == 0)
			printf("Quantum reset\n");
		break;
	case 'Q':
		q = ioctl(fd, SCULL_IOCQQUANTUM);
		printf("Quantum: %d\n", q);
		ret = 0;
		break;
	case 'G':
		ret = ioctl(fd, SCULL_IOCGQUANTUM, &q);
		if (ret == 0)
			printf("Quantum: %d\n", q);
		break;
	case 'T':
		ret = ioctl(fd, SCULL_IOCTQUANTUM, g_quantum);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'S':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCSQUANTUM, &q);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'X':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCXQUANTUM, &q);
		if (ret == 0)
			printf("Quantum exchanged, old quantum: %d\n", q);
		break;
	case 'H':
		q = ioctl(fd, SCULL_IOCHQUANTUM, g_quantum);
		printf("Quantum shifted, old quantum: %d\n", q);
		ret = 0;
		break;
	case 'i': ;//for when the input to scull is i
		task_info tinfou;
		ret = ioctl(fd, SCULL_IOCIQUANTUM, &tinfou); //call module, keep track of ret val in case of error
		if (ret == 0) {
			print_task_info(tinfou); //print the task_info struct
		}
		break;
	case 'p': ;//for when input to ./scull is p
		task_info tinfo;
		int dip;
		int dip2;
		dip = fork(); //create first fork and check for error
		if (dip == -1) {
			exit(errno);
		}
		dip2 = fork(); //create second fork
		if (dip2 == -1) {
			exit(errno);
		}
		ret = ioctl(fd, SCULL_IOCIQUANTUM, &tinfo); //run the function, should I mutex this??
		if (ret == 0) { //only if not returning an error
			print_task_info(tinfo); //print task_info
		}
		else {
			break; //break on error so perror gets thrown below with ret value
		}
		ret = ioctl(fd, SCULL_IOCIQUANTUM, &tinfo); //run module function
		if (ret == 0) {
			print_task_info(tinfo); //printtask_info if no errors
		}
		if ((dip2 > 0) && (dip > 0)) { //OG parent waits for both its children
			wait(NULL);
			wait(NULL);
		}
		else if ((dip2 != 0) && (dip == 0)) {//first child waits for its child
			wait(NULL);
			exit(0);
		}
		else { // last two children juts exit.
			exit(0);
		}
		ret = 0; // break with no error
		break;
	case 't': ;//for when input to ./scull is t
	pthread_t threads[4]; //create my list of threads of which I will run all of them
		for (int i = 0; i < 4; i++) { // for loop to create the threads.
			pthread_create(&threads[i], NULL, t_function, &fd);
		}
		for (int i = 0; i < 4; i++) { //for loop to join the threads
			pthread_join(threads[i], NULL);
		}
		ret = 0; // break with no error.
		break;
	default:
		/* Should never occur */
		abort();
		ret = -1; /* Keep the compiler happy */
	}

	if (ret != 0) {
		perror("ioctl");
	}
	return ret;
}

int main(int argc, const char **argv)
{
	int fd, ret;
	cmd_t cmd;

	cmd = parse_arguments(argc, argv);

	fd = open(CDEV_NAME, O_RDONLY);
	if (fd < 0) {
		perror("cdev open");
		return EXIT_FAILURE;
	}

	printf("Device (%s) opened\n", CDEV_NAME);

	ret = do_op(fd, cmd);

	if (close(fd) != 0) {
		perror("cdev close");
		return EXIT_FAILURE;
	}

	printf("Device (%s) closed\n", CDEV_NAME);

	return (ret != 0)? EXIT_FAILURE : EXIT_SUCCESS;
}
