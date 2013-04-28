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
#include <math.h>

#define TIMERS_ENABLED
#include "intranode_com_timers.h"

#define MAX_READ_BYTES 1048576 * 16
#define MAX_XS_PATH 100
#define WRITE_GRANULARITY 16384

unsigned int cmdline_bytes = WRITE_GRANULARITY;
unsigned int validate_enabled = 0;
unsigned int print_enabled = 0;
unsigned int starting_value = 9;

void print_usage(void)
{
	fprintf(stderr, "\nThe arguments you must provide are:\n"
		"\t1. '-s' for server or '-c' for client\n"
		"\t2. '-w' for writer or '-r' for reader\n"
		"\t3. '-d' <domID to communicate with>\n"
		"\t4. '-x' <xenstore node>\n"
		"\t5. (Only for Writer) '-b' <bytes to write>\n"
		"\t6. '-g' <write granularity> (default is 16384)\n"
		"\t7. (Only for Server) '-l' <left/right ring size>\n"
		"\t8. (Only for Writer) '-v' validate enabled\n"
		"\t8. '-p' print enabled\n"
		"\t9. '-n' for non-blocking io\n\n");
}

void initialize(int *data, int size)
{
	int i;

	for (i = 0; i < size; i++)
		data[i] = rand();
}

void print(int *data, int size)
{
	int i;
	for (i = 0; i < size; i++)
		printf("%d\n", data[i]);
}

int validate(int *wr_data, int *data, int size)
{
	int i;

	for (i = 0; i < size; i++)
		if (data[i] != wr_data[i]) {
			fprintf(stderr, "Data corruption, i = %d\n", i);
			return -1;
		}
	return 0;
}

int writer(struct libxenvchan *ctrl, void *data, int total_bytes)
{
	int written = 0, ret;
	int bytes = cmdline_bytes;
//	timers_t t1;

	while (written < total_bytes) {
//		TIMER_RESET(&t1);

//		TIMER_START(&t1);
		ret = libxenvchan_write(ctrl, data + written, bytes);
//		TIMER_STOP(&t1);

		written += ret;
		if (ret < 0) {
			perror("write");
			break;
		} else if (ret == 0) {
			fprintf(stderr, "written=%d\n", written);
			continue;
		}
//		fprintf(stderr, "vchan_write_time=%llu\n", TIMER_TOTAL(&t1));
	}

	return written;
}

struct libxenvchan *create_server_control_channel(int domid, char *xs_ctrl_path,
						  int ctrl_ring)
{

	struct libxenvchan *ctrl =
	    libxenvchan_server_init(NULL, domid, xs_ctrl_path, ctrl_ring,
				    ctrl_ring);
	return ctrl;

}

struct libxenvchan *create_client_control_channel(int domid, char *xs_ctrl_path)
{

	struct libxenvchan *ctrl =
	    libxenvchan_client_init(NULL, domid, xs_ctrl_path);
	return ctrl;

}

#define INTRANODE_CMD_DATA_SERVER_INIT		0x1
#define INTRANODE_CMD_DATA_CLIENT_INIT		0x2
#define INTRANODE_CMD_DATA_CHANNEL_DESTROY	0x3
#define INTRANODE_CMD_DATA_CLIENT_ERROR		0x4
#define INTRANODE_CMD_END			0x5

struct intranode_cmd {
	uint8_t cmd_id;
	int domid;
	char xs_path[MAX_XS_PATH];
	int total_bytes;
	int gran;
	uint32_t left_ring;
	uint32_t right_ring;
};

void
populate_cmd(struct intranode_cmd *cmd, uint8_t cmd_id, int domid,
	     char *xs_path, uint32_t left_ring, uint32_t right_ring,
	     int total_bytes, int granularity)
{

	cmd->cmd_id = cmd_id;
	cmd->domid = domid;
	sprintf(cmd->xs_path, "%s", xs_path);
	cmd->left_ring = left_ring;
	cmd->right_ring = right_ring;
	cmd->total_bytes = total_bytes;
	cmd->gran = granularity;

}

void
parse_cmd_arguments(void *read_data_from_channel, struct intranode_cmd *cmd)
{
	struct intranode_cmd *cmd_from_channel;
	cmd_from_channel = (struct intranode_cmd *)read_data_from_channel;
	memcpy(cmd, cmd_from_channel, sizeof(struct intranode_cmd));
}

int
parse_command_from_channel(struct libxenvchan *ctrl,
			   struct intranode_cmd *cmd_read,
			   struct libxenvchan **ctrl_data, int domid)
{
	struct intranode_cmd cmd;
	void *read_data_from_channel;
	timers_t t1;

	read_data_from_channel = cmd_read;
	parse_cmd_arguments(read_data_from_channel, &cmd);

	TIMER_RESET(&t1);

	switch (cmd.cmd_id) {
	case INTRANODE_CMD_DATA_SERVER_INIT:
		{
			TIMER_START(&t1);
			*ctrl_data =
			    libxenvchan_server_init(NULL, domid, cmd.xs_path,
						    cmd.left_ring,
						    cmd.right_ring);
			TIMER_STOP(&t1);
			if (!(*ctrl_data)) {
				perror("libxenvchan_data_server_init");
				return -1;
			}

			fprintf(stderr, "Data_server_init_time=%llu , ",
				TIMER_TOTAL(&t1));

			populate_cmd(&cmd, INTRANODE_CMD_DATA_CLIENT_INIT,
				     cmd.domid, cmd.xs_path, 0, 0, 0, 0);
			libxenvchan_write(ctrl, &cmd, sizeof(cmd));

			return 0;
		}
	case INTRANODE_CMD_DATA_CLIENT_INIT:
		{
			TIMER_START(&t1);
			*ctrl_data =
			    libxenvchan_client_init(NULL, domid, cmd.xs_path);
			TIMER_STOP(&t1);
			if (!(*ctrl_data)) {
				perror("libxenvchan_data_client_init");

				populate_cmd(&cmd,
					     INTRANODE_CMD_DATA_CLIENT_ERROR,
					     domid, cmd.xs_path, 0, 0, 0, 0);
				libxenvchan_write(ctrl, &cmd, sizeof(cmd));

				return -1;
			}

			fprintf(stderr, "Data_client_init_time=%llu\n",
				TIMER_TOTAL(&t1));

			return 0;
		}
	case INTRANODE_CMD_DATA_CHANNEL_DESTROY:
		{
//			fprintf(stderr, "Destroying Data channel\n\n");
			libxenvchan_close(*ctrl_data);
			return 0;
		}
	case INTRANODE_CMD_DATA_CLIENT_ERROR:
		{
			fprintf(stderr, "Data Client init error\n");
			libxenvchan_close(*ctrl_data);
			return -1;
		}
	case INTRANODE_CMD_END:
		{
			fprintf(stderr, "End of transaction. Breaking out\n");
			return -1;
		}
	}

	fprintf(stderr, "No such command\n");
	return -1;
}

int reader(struct libxenvchan *ctrl, void *data, int flag, int bytes)
{
	int size_read = 0;
	int printed = 0;
	long printing_time = 0;
//	timers_t t1;
	timers_t t2;

	while (printed < bytes) {
//		      TIMER_RESET(&t1);

/*              sleep(1);
              if ( libxenvchan_data_ready(ctrl)==0 && flag==0 ){
                      if ( libxenvchan_data_ready(ctrl)==0 ){
                              fprintf(stderr,"I am reader. No more data\n");
                              break;
                      }
              }
*/
		//TIMER_START(&t1);
		size_read =
		    libxenvchan_read(ctrl, data + printed, MAX_READ_BYTES);
		//TIMER_STOP(&t1);

//              ffprintf(stderr,stderr,"size_read = %d\n", size_read);
		if (size_read < 0)
			break;
		else if (size_read == 0)
			continue;

		//ffprintf(stderr,stderr, "vchan_read_time=%llu\n", TIMER_TOTAL(&t1));

		if (print_enabled) {
			TIMER_RESET(&t2);
			TIMER_START(&t2);
			print(data + printed, size_read / sizeof(int));
			TIMER_STOP(&t2);
			printing_time += TIMER_TOTAL(&t2);
		}

		printed += size_read;
	}

	if (print_enabled)
		fprintf(stderr, "printing time: %ld\n", printing_time);
	return printed;
}

int main(int argc, char **argv)
{
	int is_server = -1;
	struct libxenvchan *ctrl = NULL, *ctrl_data = NULL;
	int is_writer = -1;
	int domid = -1;
	int left_ring = -1, right_ring;
	int bytes = -1;
	char *xs_path = NULL;
	char *xs_data_path = malloc(sizeof(MAX_XS_PATH));
	char *xs_path_alloced = malloc(sizeof(MAX_XS_PATH));
	int c;
	int *data = malloc(MAX_READ_BYTES);
	int blocking = 1;
	timers_t t1, t2;

	while ((c = getopt(argc, argv, "scwrd:x:b:l:g:nhvpS:")) != -1)
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
		case 'n':
			blocking = 0;
			break;
		case 'v':
			validate_enabled = 1;
			break;
		case 'p':
			print_enabled = 1;
			break;
		case 'S':
                        starting_value = atoi(optarg);;
                        break;
		default:
			fprintf(stderr, "Unknown option -%c\n", c);
		case 'h':
			print_usage();
			exit(-1);
			break;
		}

	if (domid < 0) {
		print_usage();
		exit(-1);
	}

	if (!xs_path) {
		print_usage();
		exit(-1);
	}

	if (is_writer == 1 && bytes < 0) {
		print_usage();
		exit(-1);
	}
	printf("path = %s\n", xs_path);

	TIMER_RESET(&t1);
	if (is_server < 0)
		print_usage();
	else if (is_server) {
		if (left_ring < 0) {
			print_usage();
			exit(-1);
		}

		TIMER_START(&t1);
		ctrl = create_server_control_channel(domid, xs_path, 0);
		TIMER_STOP(&t1);
		fprintf(stderr, "Control_server_init_time=%llu\n",
			TIMER_TOTAL(&t1));
	} else {
		TIMER_START(&t1);
		ctrl = create_client_control_channel(domid, xs_path);
		TIMER_STOP(&t1);
		fprintf(stderr, "Control_client_init_time=%llu\n",
			TIMER_TOTAL(&t1));
	}

	if (!ctrl) {
		perror("libxenvchan_init");
		return 0;
	}

	ctrl->blocking = blocking;

	snprintf(xs_path_alloced, strlen(xs_path), "%s", xs_path);
	strcat(xs_data_path, xs_path);
	strncat(xs_data_path, "/data_channel", strlen("/data_channel"));
	printf("data_path = %s\n", xs_data_path);
	printf("path = %s\n", xs_path);

	if (is_writer) {
		int size, wr;
		int *wr_data = NULL;

		while (1) {
			struct intranode_cmd cmd;

			do {
				libxenvchan_read(ctrl, &cmd, sizeof(cmd));
				if (parse_command_from_channel
				    (ctrl, &cmd, &ctrl_data, domid) < 0) {
					goto out_with_corruption;
				}
				sleep(1);
			}
			while (libxenvchan_data_ready(ctrl) > 0);

			ctrl_data->blocking = 1;

			bytes = cmd.total_bytes;
			size = bytes / sizeof(int);
			cmdline_bytes = cmd.gran;

			TIMER_RESET(&t1);

			TIMER_START(&t1);
			wr_data = malloc(bytes);
			initialize(wr_data, size);
			TIMER_STOP(&t1);

			fprintf(stderr,"bytes=%d , data_initialization_time=%llu\n",bytes, TIMER_TOTAL(&t1));

			wr = writer(ctrl_data, wr_data, bytes);

			reader(ctrl_data, data, is_writer, bytes);

			if (validate_enabled) {
				if (validate(wr_data, data, size)) {
					free(wr_data);
					goto out_with_corruption;
				}
			}
			free(wr_data);

			libxenvchan_read(ctrl, &cmd, sizeof(cmd));
			if (parse_command_from_channel
			    (ctrl, &cmd, &ctrl_data, domid) < 0) {
				free(wr_data);
				goto out_with_corruption;
			}

		}
	}

	if (!is_writer) {
		struct intranode_cmd cmd;
		int total_bytes = starting_value;
		int gran;
		int ring_size;

		while (total_bytes < 25) {
			bytes = (int)pow(2, total_bytes);
			ring_size = starting_value;
			while ((ring_size <= total_bytes) && (ring_size <= 20)) {
				left_ring = right_ring = (int)pow(2, ring_size++);
				gran = starting_value;
				while (gran <= total_bytes) {

					TIMER_RESET(&t1);
					TIMER_RESET(&t2);

					cmdline_bytes = (int)pow(2, gran++);
					populate_cmd(&cmd,
						     INTRANODE_CMD_DATA_SERVER_INIT,
						     domid, xs_data_path,
						     left_ring, right_ring,
						     bytes, cmdline_bytes);
					libxenvchan_write(ctrl, &cmd,
							  sizeof(cmd));

					libxenvchan_read(ctrl, &cmd,
							 sizeof(cmd));
					if (parse_command_from_channel
					    (ctrl, &cmd, &ctrl_data, domid) < 0)
						goto out_with_corruption;

					ctrl_data->blocking = 1;

					sleep(2);

					TIMER_START(&t1);
					bytes =
					    reader(ctrl_data, data, is_writer,
						   bytes);
					TIMER_STOP(&t1);

					TIMER_START(&t2);
					writer(ctrl_data, data, bytes);
					TIMER_STOP(&t2);

					fprintf(stderr,
						"bytes=%d , gran=%d , ring=%d , Reader_time=%llu , Writer_time=%llu\n\n",
						bytes, cmdline_bytes, left_ring,
						TIMER_TOTAL(&t1),
						TIMER_TOTAL(&t2));

					populate_cmd(&cmd,
						     INTRANODE_CMD_DATA_CHANNEL_DESTROY,
						     domid, xs_data_path, 0, 0,
						     0, 0);
					libxenvchan_write(ctrl, &cmd,
							  sizeof(cmd));
					libxenvchan_close(ctrl_data);
				}
			}
			total_bytes++;
		}
		populate_cmd(&cmd, INTRANODE_CMD_END, domid, xs_data_path, 0, 0,
			     0, 0);
		libxenvchan_write(ctrl, &cmd, sizeof(cmd));
	}

out_with_corruption:
	free(data);
	libxenvchan_close(ctrl);
	return 0;
}
