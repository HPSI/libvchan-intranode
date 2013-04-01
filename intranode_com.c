#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libxenvchan.h>
#include <getopt.h>

//#define TYPE int
#define MAX_READ_BYTES 1048576
#define WRITE_GRANULARITY 16384

unsigned int cmdline_bytes = WRITE_GRANULARITY;

void print_usage(void)
{
	fprintf(stderr,"\nThe arguments you must provide are:\n"
			"\t1. '-s' for server or '-c' for client\n"
			"\t2. '-wr' for writer or '-rd' for reader\n"
			"\t3. The domID to communicate with\n"
			"\t4. The xenstore node to use\n\n");
}

void initialize(int *data, int size)
{
	int i;
	
	for(i=0;i<size;i++)
//		data[i]=rand();
		data[i]=i;
}

void print(int *data , int size)
{
        int i;
        for(i=0;i<size;i++)
                printf("%d\n",data[i]);
}

int writer(struct libxenvchan *ctrl, void *data, int total_bytes)
{
	int written = 0,ret;
	int bytes = cmdline_bytes;
	
	while(written<total_bytes){
		ret = libxenvchan_write(ctrl,data+written,bytes);
		printf("ret=%d\n",ret);
		if (ret<=0){
			perror("write");
			exit(1);
		}
		written += ret;
	}
	printf("written=%d\n",written);

	free(data);
	return written;
}

int reader(struct libxenvchan *ctrl)
{
	int size_read=0;
	int printed=0;
	int *data = malloc(MAX_READ_BYTES);

	while(1){
		size_read = libxenvchan_read(ctrl, data, MAX_READ_BYTES);
		if (size_read < 0) {
			break;
		}
		print(data,size_read/sizeof(int));
		printed+=size_read;
	}
	
	printf ("Read total of %d bytes\n",printed);
	free(data);
	return printed;
}

int main(int argc , char **argv)
{
	int is_server;
	struct libxenvchan *ctrl;
	int is_writer;
	int domid;
	int left_ring , right_ring;
	char *endptr;
	char *xs_path;
	int c;

	while ((c = getopt(argc, argv, "scwrd:x:")) != -1)
	  switch (c) {
	  case 's':
	    is_server = 1;
	    break;
	  case 'c':
	    is_server = 0;
	    break;
	  case 'w':
	    is_writer = 1;
	    break;
	  case 'r':
	    is_writer = 0;
	    break;
	  case 'd':
	    domid = atoi(optarg);
	    break;
	  case 'x':
	    xs_path = strdup(optarg);
	    break;
	  default:
	    fprintf(stderr, "Unknown option -%c\n", c);
	  case 'h':
	    print_usage();
	    exit(-1);
	    break;
	  }
	

#if 0
	if (!strcmp(argv[1],"-s")) is_server=1;
	else if (!strcmp(argv[1],"-c")) is_server=0;
	else {
		fprintf(stderr,"\nError in 1st argument\n");
		print_usage();
		return 0;
	}

	if (!strcmp(argv[2],"-wr")) is_writer=1;
	else if (!strcmp(argv[2],"-rd")) is_writer=0;
	else {
		fprintf(stderr,"\nError in 2nd argument\n");
                print_usage();
                return 0;
        }

	domid = strtol(argv[3], &endptr, 10);
	if (strcmp(endptr,"")) {
		fprintf(stderr, "\nError in 3rd argument\n");
		print_usage();
                return 0;
	}
#endif

/*	left_ring = strtol(argv[5], &endptr, 10);
	if (strcmp(endptr,"")) {
                fprintf(stderr, "\nError in 5th argument\n");
                print_usage();
                return 0;
        }
	right_ring = left_ring;
*/
	if (is_server){
		printf("Give the size of the read/write rings: ");
		scanf("%d",&left_ring);
		right_ring = left_ring;
		ctrl = libxenvchan_server_init(NULL,domid,xs_path,left_ring,right_ring);
	}
	else
		ctrl = libxenvchan_client_init(NULL,domid,xs_path);

	if (!ctrl){
		perror("libxenvchan_init");
		return 0;
	}
	ctrl->blocking=1;
	printf("init is ok\n");

	if (is_writer) {
		int size = 0; 
		int *data; 
		int bytes;
		printf("You are the writer.\nProvide the bytes you want to write: ");
		scanf("%d",&bytes);

		size = bytes/sizeof(int);
		data = malloc(bytes);
		initialize(data, size);

		writer(ctrl,data, bytes);
	}	
	else reader(ctrl);

	libxenvchan_close(ctrl);
	return 0;
}
