#include "_file_stream.c"
#include "rtp_h264.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// get current time (millsecond)
static unsigned long
get_cur_time()
{
	unsigned long msec;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	msec =
		((unsigned long)tv.tv_sec * 1000) + ((unsigned long)tv.tv_usec / 1000);

	return msec;
}

int
main()
{
	int sock;
	struct sockaddr_in sockaddr;

	int port = 6000;
	char* addr = "127.0.0.1";

	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_addr.s_addr = inet_addr(addr);
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);

	printf("socket configuration\n");

	// socket configuration
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	FILE* file =
		fopen("/home/matthew/Documents/git_repos/rtpencoder/test.264", "rb");
	if( NULL == file )
	{
		printf("failed to open file\n");
		return -1;
	}

	char buf[1920 * 1080];
	int start_time = get_cur_time();
	bool eof = false;
	char packet[1500];
	struct RTPStreamInfo stream_info = {0};
	rtp_h264_stream_init(&stream_info, RTP_PAYLOAD_KIND_H264, 0);

	struct RTPH264Packetization packetization = {0};
	while( true )
	{
		memset(buf, 0, sizeof(buf));
		memset(packet, 0, sizeof(packet));
		memset(&packetization, 0, sizeof(packetization));

		int read_len = read_packet(file, buf, &eof);
		if( read_len <= 0 )
			break;

		if( read_len > 0 && false == eof )
		{
			// framerate control
			usleep(1000 * 30);
			int time = (get_cur_time() - start_time) * 90;
			rtp_h264_packetize_begin(
				&stream_info, &packetization, 1500, buf, read_len);

			while( !rtp_h264_packetize_is_done(&packetization) )
			{
				int packet_len = rtp_h264_packetize_next(
					&stream_info, &packetization, packet, time);
				if( packet_len < 0 )
					return -1;

				int bytes_sent = sendto(
					sock,
					packet,
					packet_len,
					0,
					(struct sockaddr*)&sockaddr,
					(socklen_t)sizeof(sockaddr));
			}
		}
		else
		{
			break;
		}
	}

	return 0;
}