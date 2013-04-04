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
#include <sys/time.h>

#define MAX_READ_BYTES 1048576
#define WRITE_GRANULARITY 16384

unsigned int cmdline_bytes = WRITE_GRANULARITY;

void print_usage(void)
{
	fprintf(stderr,"\nThe arguments you must provide are:\n"
			"\t1. '-s' for server or '-c' for client\n"
			"\t2. '-w' for writer or '-r' for reader\n"
			"\t3. '-d' <domID to communicate with>\n"
			"\t4. '-x' <xenstore node>\n"
			"\t5. (Only for Writer) '-b' <bytes to write>\n"
			"\t6. (Only for Writer) '-g' <write granularity> (default is 16384)\n"
			"\t7. (Only for Server) '-l' <left/right ring size>\n\n");
}

void initialize(int *data, int size)
{
	int i;
	
	for(i=0;i<size;i++)
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
	struct timeval start,stop;
	
	while(written<total_bytes){
		gettimeofday(&start,NULL);
		ret = libxenvchan_write(ctrl,data+written,bytes);
		gettimeofday(&stop,NULL);
//		printf("ret=%d\n",ret);
		if (ret<=0){
			perror("write");
			exit(1);
		}
		fprintf(stderr,"vchan_write time: %.3f\n",(double)((stop.tv_sec+(double)stop.tv_usec/1000)-(start.tv_sec+(double)start.tv_usec/1000)));
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
	struct timeval start,stop;

	while(1){
		gettimeofday(&start,NULL);
		size_read = libxenvchan_read(ctrl, data, MAX_READ_BYTES);
		gettimeofday(&stop,NULL);
		if (size_read < 0) break;
		fprintf(stderr,"vchan_read time: %.3f\n",(double)((stop.tv_sec+(double)stop.tv_usec/1000)-(start.tv_sec+(double)start.tv_usec/1000)));
		print(data,size_read/sizeof(int));
		printed+=size_read;
	}
	
	printf ("Read total of %d bytes\n",printed);
	free(data);
	return printed;
}

int main(int argc , char **argv)
{
	int is_server = -1;
	struct libxenvchan *ctrl = NULL;
	int is_writer = -1;
	int domid = -1;
	int left_ring = -1 , right_ring;
	int bytes = -1;
	char *xs_path = NULL;
	int c;
	struct timeval start,stop,wr_start,wr_stop;

	while ((c = getopt(argc, argv, "scwrd:x:b:l:g:h")) != -1)
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
	  case 'b':
	    bytes = atoi(optarg);
	    break;
	  case 'l':
	    left_ring = atoi(optarg);
	    break;
	  case 'g':
	    cmdline_bytes = atoi(optarg);
	    break;
	  default:
	    fprintf(stderr, "Unknown option -%c\n", c);
	  case 'h':
	    print_usage();
	    exit(-1);
	    break;
	  }
	
	if (domid<0){
		print_usage();
		exit(-1);
	}

	if (!xs_path){
		print_usage();
		exit(-1);
	}

	if (is_server<0) print_usage();
	else if (is_server){
		if (left_ring<0){
			print_usage();
			exit(-1);
		}
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

	if (is_writer<0) print_usage();
	else if (is_writer) {
		int size = 0; 
		int *data; 

		if (bytes<0){
			print_usage();
                        exit(-1);
                }

		size = bytes/sizeof(int);
		gettimeofday(&start,NULL);
		data = malloc(bytes);
		initialize(data, size);
		gettimeofday(&stop,NULL);
		printf("initialization time: %.3f\n",(double)((stop.tv_sec+(double)stop.tv_usec/1000)-(start.tv_sec+(double)start.tv_usec/1000)));

		gettimeofday(&wr_start,NULL);
		writer(ctrl,data, bytes);
		gettimeofday(&wr_stop,NULL);
		printf("writer time: %.3f\n",(double)((wr_stop.tv_sec+(double)wr_stop.tv_usec/1000)-(wr_start.tv_sec+(double)wr_start.tv_usec/1000)));
	}	
	else{
		gettimeofday(&start,NULL);
		reader(ctrl);
		gettimeofday(&stop,NULL);
		fprintf(stderr,"reader time: %.3f\n",(double)((stop.tv_sec+(double)stop.tv_usec/1000)-(start.tv_sec+(double)start.tv_usec/1000)));
	}

	libxenvchan_close(ctrl);
	return 0;
}