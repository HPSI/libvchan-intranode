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

#define TIMERS_ENABLED
#include "intranode_com_timers.h"

#define MAX_READ_BYTES 1048576 * 16
#define WRITE_GRANULARITY 16384

unsigned int cmdline_bytes = WRITE_GRANULARITY;
unsigned int cmdline_nrtimes = 1;

void print_usage(void)
{
	fprintf(stderr,"\nThe arguments you must provide are:\n"
			"\t1. '-s' for server or '-c' for client\n"
			"\t2. '-w' for writer or '-r' for reader\n"
			"\t3. '-d' <domID to communicate with>\n"
			"\t4. '-x' <xenstore node>\n"
			"\t5. (Only for Writer) '-b' <bytes to write>\n"
			"\t6. '-g' <write granularity> (default is 16384)\n"
			"\t7. (Only for Server) '-l' <left/right ring size>\n"
			"\t8. '-n' for non-blocking io\n\n");
}

void initialize(int *data, int size)
{
	int i;
	
	for(i=0;i<size;i++)
		data[i]=rand();
}

void print(int *data , int size)
{
        int i;
        for(i=0;i<size;i++)
                printf("%d\n",data[i]);
}

int validate(int *wr_data, int *data, int size)
{
	int i;
	
	for(i=0;i<size;i++)
		if (data[i]!=wr_data[i]) {
			fprintf(stderr, "Data corruption, i = %d\n", i);
			return -1;
                }
	return 0;
}

int writer(struct libxenvchan *ctrl, void *data, int total_bytes)
{
	int written = 0,ret;
	int bytes = cmdline_bytes;
//	struct timeval start,stop;
	timers_t t1;
		
	while(written<total_bytes){
		TIMER_RESET(&t1);
//		gettimeofday(&start,NULL);
		TIMER_START(&t1);
		ret = libxenvchan_write(ctrl,data+written,bytes);
		TIMER_STOP(&t1);
//		gettimeofday(&stop,NULL);
//		fprintf(stderr,"ret = %d\n",ret);
		written += ret;
		if (ret<0){
			perror("write");
//			printf("ret=%d\n",ret);
			break;
		}
		else if (ret == 0) {
			fprintf(stderr, "written=%d\n",written);
			continue;
		}
                fprintf(stderr, "vchan_write timer count= %lu, total = %llu\n",TIMER_COUNT(&t1), TIMER_TOTAL(&t1));
//		fprintf(stderr,"vchan_write time: %ld\n",(stop.tv_sec*1000000+stop.tv_usec)-(start.tv_sec*1000000+start.tv_usec));
	}

	return written;
}

int reader(struct libxenvchan *ctrl, void *data, int flag, int bytes)
{
	int size_read=0;
	int printed=0;
	long printing_time=0;
//	struct timeval start,stop;
	timers_t t1,t2;

	while(1){
		TIMER_RESET(&t1);
		TIMER_RESET(&t2);
//		fprintf(stderr,"data_ready = %d\n",libxenvchan_data_ready(ctrl));
		if ( libxenvchan_data_ready(ctrl)==0 && flag==0 ) break;
		
//		gettimeofday(&start,NULL);
		TIMER_START(&t1);
		size_read = libxenvchan_read(ctrl, data+printed, MAX_READ_BYTES);
		TIMER_STOP(&t1);
//		gettimeofday(&stop,NULL);
		
//		fprintf(stderr,"size_read = %d\n", size_read);
		if (size_read < 0) break;
		else if (size_read == 0) continue;
		
                fprintf(stderr, "vchan_read timer count= %lu, total = %llu\n",TIMER_COUNT(&t1), TIMER_TOTAL(&t1));
		//fprintf(stderr,"vchan_read time: %ld\n",(stop.tv_sec*1000000+stop.tv_usec)-(start.tv_sec*1000000+start.tv_usec));

//		gettimeofday(&start,NULL);
		TIMER_START(&t2);
		print(data+printed,size_read/sizeof(int));
		TIMER_STOP(&t2);
//		gettimeofday(&stop,NULL);
//		printing_time += (stop.tv_sec*1000000+stop.tv_usec)-(start.tv_sec*1000000+start.tv_usec);
		printing_time += TIMER_TOTAL(&t2);

		printed+=size_read;
		if (bytes>0 && printed>=bytes) break;
	}
//	fprintf(stderr,"printing time: %ld\n",printing_time);
	printf ("Read total of %d bytes\n",printed);
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
	int *data = malloc(MAX_READ_BYTES);
	int blocking=1;
//	struct timeval start,stop;
	timers_t t1;
	int j;

	while ((c = getopt(argc, argv, "scwrd:x:b:l:g:nhN:")) != -1)
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
	  case 'N':
	    cmdline_nrtimes = atoi(optarg);
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
	  case 'n':
	    blocking = 0;
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

        if (is_writer==1 && bytes<0){
		print_usage();
                exit(-1);
        }

	TIMER_RESET(&t1);
	if (is_server<0) print_usage();
	else if (is_server){
		if (left_ring<0){
			print_usage();
			exit(-1);
		}
		right_ring = left_ring;
//		gettimeofday(&start,NULL);
		TIMER_START(&t1);
		ctrl = libxenvchan_server_init(NULL,domid,xs_path,left_ring,right_ring);
		TIMER_STOP(&t1);
		fprintf(stderr, "server_init timer count= %lu, total = %llu\n",TIMER_COUNT(&t1), TIMER_TOTAL(&t1));
//		gettimeofday(&stop,NULL);
//		fprintf(stderr,"server_init time: %ld\n",(stop.tv_sec*1000000+stop.tv_usec)-(start.tv_sec*1000000+start.tv_usec));
	}
	else{
//		gettimeofday(&start,NULL);
		TIMER_START(&t1);
		ctrl = libxenvchan_client_init(NULL,domid,xs_path);
		TIMER_STOP(&t1);
                fprintf(stderr, "client_init timer count= %lu, total = %llu\n",TIMER_COUNT(&t1), TIMER_TOTAL(&t1));
//		gettimeofday(&stop,NULL);
//                fprintf(stderr,"client_init time: %ld\n",(stop.tv_sec*1000000+stop.tv_usec)-(start.tv_sec*1000000+start.tv_usec));
	}
	if (!ctrl){
		perror("libxenvchan_init");
		return 0;
	}

	ctrl->blocking = blocking;

	printf("init is ok\n");

	if (is_writer<0) print_usage();
	else if (is_writer) {
		int size = bytes/sizeof(int);
		int *wr_data;
		timers_t t1, t2;
		
		while (1) {
			TIMER_RESET(&t1); 
			TIMER_RESET(&t2); 

//			gettimeofday(&start,NULL);
			wr_data = malloc(bytes);
			initialize(wr_data, size);
//			gettimeofday(&stop,NULL);
			//fprintf(stderr,"data initialization time: %ld\n",(stop.tv_sec*1000000+stop.tv_usec)-(start.tv_sec*1000000+start.tv_usec));

			TIMER_START(&t1); 
//			gettimeofday(&start,NULL);
			writer(ctrl, wr_data, bytes);
//			gettimeofday(&stop,NULL);
			TIMER_STOP(&t1); 
//			fprintf(stderr,"writer time: %ld\n",(stop.tv_sec*1000000+stop.tv_usec)-(start.tv_sec*1000000+start.tv_usec));
			fprintf(stderr,"I am the writer. Finished writing things\n");
			
			TIMER_START(&t2); 
			reader(ctrl, data, is_writer,bytes);
			TIMER_STOP(&t2); 
			if (validate(wr_data, data, size)) {
				free(wr_data);
				goto out_with_corruption;
			}
			free(wr_data);

			fprintf(stderr,"I am the writer. Finished reading things\n");

		}
		fprintf(stderr, "write timer count= %lu, total = %llu\n",TIMER_COUNT(&t1), TIMER_TOTAL(&t1));
		fprintf(stderr, "read timer count = %lu, total = %llu\n", TIMER_COUNT(&t2), TIMER_TOTAL(&t2));
	}	
	else {
		int terminate=0;
		timers_t t1, t2;
//		for (j = 0; j < cmdline_nrtimes ; j++) {
		while(terminate>=0){
			TIMER_RESET(&t1);
                	TIMER_RESET(&t2);

			TIMER_START(&t2); 
//			gettimeofday(&start,NULL);
			bytes = reader(ctrl, data, is_writer,-1);
//			gettimeofday(&stop,NULL);
			TIMER_STOP(&t2); 
//			fprintf(stderr,"reader time: %ld\n",(stop.tv_sec*1000000+stop.tv_usec)-(start.tv_sec*1000000+start.tv_usec));
			
			TIMER_START(&t1); 
			writer(ctrl, data, bytes);
			TIMER_STOP(&t1);
			fprintf(stderr, "write timer count= %lu, total = %llu\n",TIMER_COUNT(&t1), TIMER_TOTAL(&t1));
			fprintf(stderr, "read timer count = %lu, total = %llu\n", TIMER_COUNT(&t2), TIMER_TOTAL(&t2));
			scanf("%d",&terminate);
		}
	}

out_with_corruption:
	free(data);
	libxenvchan_close(ctrl);
	return 0;
}
