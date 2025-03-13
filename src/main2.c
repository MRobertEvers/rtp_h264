#include "rtpencoder.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MTU_SIZE 1500
#define RTP_H264_MAX_PKT_SIZE MTU_SIZE
#define RTP_HDR_SIZE 12
#define RTP_H264_MAX_NAL_DATA_SIZE (RTP_H264_MAX_PKT_SIZE - RTP_HDR_SIZE)

int sock;
struct sockaddr_in sockaddr;

enum RTPH264HeaderKind
{
	RTPH264_HEADER_KIND_STAP_A = 24,
	RTPH264_HEADER_KIND_STAP_B = 25,
	RTPH264_HEADER_KIND_MTAP16 = 26,
	RTPH264_HEADER_KIND_MTAP24 = 27,
	RTPH264_HEADER_KIND_FU_A = 28,
	RTPH264_HEADER_KIND_FU_B = 29
};

#define NALU_HEADER_TYPE(byte) ((byte) & 0x1f)
#define NALU_HEADER_NRI(byte) (((byte) & 0x60) >> 5)
#define NALU_HEADER_F(byte) (((byte) & 0x80) >> 7)

#define FU_INDICATOR_TYPE(byte) ((byte) & 0x1f)
#define FU_INDICATOR_NRI(byte) (((byte) & 0x60) >> 5)
#define FU_INDICATOR_F(byte) (((byte) & 0x80) >> 7)

#define FU_HEADER_TYPE(byte) ((byte) & 0x1f)
#define FU_HEADER_R(byte) (((byte) & 0x40) >> 6)
#define FU_HEADER_E(byte) (((byte) & 0x20) >> 5)
#define FU_HEADER_S(byte) (((byte) & 0x80) >> 7)

typedef struct nalu_header
{
	uint8_t type : 5;
	uint8_t nri : 2;
	uint8_t f : 1;
} __attribute__((packed)) nalu_header_t;

typedef struct nalu
{
	int startcodeprefix_len;
	unsigned len;
	unsigned max_size;
	int forbidden_bit;
	int nal_reference_idc;
	int nal_unit_type;
	char* buf;
	unsigned short lost_packets;
} nalu_t;

typedef struct fu_indicator
{
	uint8_t type : 5;
	uint8_t nri : 2;
	uint8_t f : 1;
} __attribute__((packed)) fu_indicator_t;

typedef struct fu_header
{
	uint8_t type : 5;
	uint8_t r : 1;
	uint8_t e : 1;
	uint8_t s : 1;
} __attribute__((packed)) fu_header_t;

int
rtp_encode(
	uint8_t* rtp_packet,
	uint8_t* data,
	int len,
	int time,
	enum RTPH264Mode mode,
	uint8_t cu_pack_num,
	uint8_t fu_pack_num,
	struct RTPPacketInfo* pkt_info)
{
	uint8_t marker_bit = 0;
	uint32_t last_pkt_size = 0;
	nalu_header_t* nalu_hdr;
	fu_header_t* fu_hdr;
	fu_indicator_t* fu_ind;

	if( !data || !len )
	{
		printf("Encode Invalid params or no Data to encode");
		return -1;
	}

	// RTP timestamp with 90000 scale
	int index = 0;

	// version = 2
	rtp_packet[index++] = (uint8_t)(2 << 6);

	marker_bit =
		(RTPH264_MODE_FU_A_MODE == mode && cu_pack_num == fu_pack_num - 1) ? 1
																		   : 0;

	// marker bit
	rtp_packet[index] = (uint8_t)(marker_bit << 7);

	rtp_packet[index++] |= (uint8_t)pkt_info->payloadtype;

	rtp_packet[index++] = (uint8_t)(pkt_info->sequenceno >> 8);

	rtp_packet[index++] = (uint8_t)(pkt_info->sequenceno);

	pkt_info->sequenceno += 1;

	// Timestamp
	rtp_packet[index++] = (uint8_t)(time >> 24);
	rtp_packet[index++] = (uint8_t)(time >> 16);
	rtp_packet[index++] = (uint8_t)(time >> 8);
	rtp_packet[index++] = (uint8_t)(time >> 0);

	// TX SSRC
	rtp_packet[index++] = (uint8_t)(pkt_info->ssrc >> 24);
	rtp_packet[index++] = (uint8_t)(pkt_info->ssrc >> 16);
	rtp_packet[index++] = (uint8_t)(pkt_info->ssrc >> 8);
	rtp_packet[index++] = (uint8_t)(pkt_info->ssrc >> 0);

	// NALU header.
	switch( mode )
	{
	case RTPH264_MODE_SINGLE_NAL_MODE:
		nalu_hdr = (nalu_header_t*)&rtp_packet[index];
		nalu_hdr->f = (data[0] & 0x80) >> 7;   /* bit0 */
		nalu_hdr->nri = (data[0] & 0x60) >> 5; /* bit1~2 */
		nalu_hdr->type = (data[0] & 0x1f);
		index++;
		memcpy(rtp_packet + index, data + 1, len - 1); /* skip the first byte */
		return len + index - 1;
	case RTPH264_MODE_FU_A_MODE:
		fu_ind = (fu_indicator_t*)&rtp_packet[index];
		fu_ind->f = (data[0] & 0x80) >> 7;
		fu_ind->nri = (data[0] & 0x60) >> 5;
		fu_ind->type = 28;
		index++;
		// first FU-A package
		if( cu_pack_num == 0 )
		{
			fu_hdr = (fu_header_t*)&rtp_packet[index];
			fu_hdr->s = 1;
			fu_hdr->e = 0;
			fu_hdr->r = 0;
			fu_hdr->type = data[0] & 0x1f;
			index++;
			memcpy(
				rtp_packet + index, data + 1, RTP_H264_MAX_NAL_DATA_SIZE - 1);
			return index + RTP_H264_MAX_NAL_DATA_SIZE - 1;
		}
		// between FU-A package
		else if( cu_pack_num < fu_pack_num - 1 )
		{
			fu_hdr = (fu_header_t*)&rtp_packet[index];
			fu_hdr->s = 0;
			fu_hdr->e = 0;
			fu_hdr->r = 0;
			fu_hdr->type = data[0] & 0x1f;
			index++;
			memcpy(
				rtp_packet + index,
				data + cu_pack_num * RTP_H264_MAX_NAL_DATA_SIZE,
				RTP_H264_MAX_NAL_DATA_SIZE);
			return index + RTP_H264_MAX_NAL_DATA_SIZE;
		}
		// last FU-A package
		else
		{
			fu_hdr = (fu_header_t*)&rtp_packet[index];
			fu_hdr->s = 0;
			fu_hdr->e = 1;
			fu_hdr->r = 0;
			fu_hdr->type = data[0] & 0x1f;
			index++;
			last_pkt_size = len % RTP_H264_MAX_NAL_DATA_SIZE
								? len % RTP_H264_MAX_NAL_DATA_SIZE
								: RTP_H264_MAX_NAL_DATA_SIZE;
			memcpy(
				rtp_packet + index,
				data + cu_pack_num * RTP_H264_MAX_NAL_DATA_SIZE,
				last_pkt_size);
			return index + last_pkt_size;
		}
	}
}

#define RTP_READ_FILE_BYTES (1024 * 1024)

bool
find_nal_startcode(uint8_t* buf, uint32_t len)
{
	bool bFound = false;

	// The start code could be 3 or 4 bytes
	if( len == 3 )
	{
		if( *buf == 0x0 && *(buf + 1) == 0x0 && *(buf + 2) == 0x1 )
			bFound = true;
	}
	else if( len == 4 )
	{
		if( *buf == 0x0 && *(buf + 1) == 0x0 && *(buf + 2) == 0x0 &&
			*(buf + 3) == 0x1 )
			bFound = true;
	}
	else
	{
		printf("Invalid start code len\n");
	}

	return bFound;
}

/* function to read a complete
 *
 * return valve:
 * -1   -  invalid bufffer or start code
 *  0   -  need more data to complete a nal
 *  >0  -  nal length
 */
uint32_t
parse_nal(uint8_t* buf, uint32_t len, uint32_t* startcode_len)
{
	uint32_t ret_len = 0;

	if( NULL == buf || len <= 3 )
	{
		printf("invalid input buffer, size: %d\n", len);
		return -1;
	}

	if( true == find_nal_startcode(buf, 3) )
	{
		*startcode_len = 3;
	}
	else if( true == find_nal_startcode(buf, 4) )
	{
		*startcode_len = 4;
	}
	else
	{
		printf("error: cannot find start code\n");
		return -1;
	}

	// If we find the next start code, then we are done
	for( uint32_t i = 0; i < len - *startcode_len; i++ )
	{
		if( true ==
			find_nal_startcode(buf + *startcode_len + i, *startcode_len) )
		{
			ret_len = i + *startcode_len;
			break;
		}
	}

	return ret_len;
}

int
read_packet(FILE* file, char* buf, bool* eof)
{
	uint32_t read_len = 0, parse_len = 0, startcode_len = 0;
	static uint32_t offset = 0;
	static uint8_t buf_read[RTP_READ_FILE_BYTES * 10] = {0};
	*eof = false;
	enum RTPPayloadKind payload_type = RTP_PAYLOAD_KIND_H264;

	if( payload_type == RTP_PAYLOAD_KIND_H264 )
	{
		if( offset > 0 )
		{
			// We still have remaining data in buf_read, parse it
			parse_len = parse_nal(buf_read, offset, &startcode_len);
			if( parse_len > startcode_len )
			{
				memcpy(
					buf, buf_read + startcode_len, parse_len - startcode_len);
				offset = offset - parse_len;
				memcpy(buf_read, buf_read + parse_len, offset);
				return parse_len;
			}
		}

		while( parse_len <= 0 )
		{
			read_len = fread(buf_read + offset, 1, RTP_READ_FILE_BYTES, file);
			printf("read FU: %d\n", read_len);
			for( int i = 0; i < 20; i++ )
			{
				printf("%02x ", *(buf_read + offset + 1));
			}
			printf("\n");

			printf("Reading %d\n", read_len);
			if( read_len <= 0 )
			{
				if( offset > 0 )
				{
					// last nal cannot be parsed, just return remaining bytes
					memcpy(buf, buf_read, offset);
					parse_len = offset;
					offset = 0;
					return parse_len;
				}
				else
				{
					// reach EOF or error happened, finish reading
					goto finished;
				}
			}
			else
			{
				parse_len =
					parse_nal(buf_read, offset + read_len, &startcode_len);
				if( parse_len > startcode_len )
				{
					memcpy(
						buf,
						buf_read + startcode_len,
						parse_len - startcode_len);
					offset = offset + read_len - parse_len;
					memcpy(buf_read, buf_read + parse_len, offset);
					printf("parse_len: %d, %d\n", parse_len, offset);
				}
				else if( parse_len == 0 )
				{
					offset += read_len;
				}
				else
				{
					// Invalid buffer or buffer length
					return -1;
				}
			}
		}
	}
	else
	{
		printf("unsupport payload type: %d\n", payload_type);
		return -1;
	}

	return parse_len - startcode_len;

finished:
	*eof = true;
	memset(buf, 0, sizeof(buf));
	return 0;
}

// get current time (millsecond)
unsigned long
get_cur_time()
{
	unsigned long msec;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	msec =
		((unsigned long)tv.tv_sec * 1000) + ((unsigned long)tv.tv_usec / 1000);

	return msec;
}
uint32_t
send_packet(uint8_t* rtp_pkt, uint32_t len)
{
	uint32_t bytes_sned = 0;

	if( NULL != rtp_pkt && len > 0 )
	{
		bytes_sned = sendto(
			sock,
			rtp_pkt,
			len,
			0,
			(struct sockaddr*)&sockaddr,
			(socklen_t)sizeof(sockaddr));

		printf("bytes_sned: %d\n", bytes_sned);
		if( bytes_sned == 0 || bytes_sned == -1 )
		{
			printf("rtpencoder failed to send packet, error\n");
			return -1;
		}
	}
	else
	{
		printf("Invalid rtp packet\n");
		return -1;
	}

	return bytes_sned;
}

uint32_t
encode_stream_pkt(
	uint8_t* packet,
	uint8_t* data,
	uint32_t data_len,
	uint32_t time,
	struct RTPPacketInfo* pkt_info)
{
	uint8_t fu_pack_num; // FU-A total package number
	uint8_t cu_pack_num; // FU-A current package number for processing
	uint32_t pkt_offset = RTP_HDR_SIZE; // RTP pkt header offset
	uint32_t pkt_size = 0;
	uint32_t bytes_sent = 0;	// total sent bytes
	uint32_t bytes_sent_cu = 0; // current package sent bytes

	enum RTPH264Mode mode = (data_len < RTP_H264_MAX_NAL_DATA_SIZE)
								? RTPH264_MODE_SINGLE_NAL_MODE
								: RTPH264_MODE_FU_A_MODE;

	if( RTPH264_MODE_SINGLE_NAL_MODE == mode )
	{
		pkt_size =
			rtp_encode(packet, data, data_len, time, mode, 0, 0, pkt_info);
		printf("pkt_size SINGLE: %d\n", pkt_size);
		bytes_sent = send_packet(packet, pkt_size);
	}
	else if( RTPH264_MODE_FU_A_MODE == mode )
	{
		fu_pack_num = data_len % RTP_H264_MAX_NAL_DATA_SIZE
						  ? (data_len / RTP_H264_MAX_NAL_DATA_SIZE + 1)
						  : data_len / RTP_H264_MAX_NAL_DATA_SIZE;

		cu_pack_num = 0;
		while( cu_pack_num < fu_pack_num )
		{
			// print first few bytes of data
			printf("data: %d\n", data_len);
			for( int i = 0; i < 20; i++ )
			{
				printf("%02x ", data[i]);
			}
			printf("\n");
			pkt_size = rtp_encode(
				packet,
				data,
				data_len,
				time,
				mode,
				cu_pack_num,
				fu_pack_num,
				pkt_info);

			printf("pkt_size FU: %d\n", pkt_size);
			for( int i = 0; i < 20; i++ )
			{
				printf("%02x ", packet[i]);
			}
			printf("\n");

			bytes_sent_cu = send_packet(packet, pkt_size);

			bytes_sent += bytes_sent_cu;
			cu_pack_num++;
		}
	}

	return bytes_sent;
}

int
main()
{
	enum RTPPayloadKind payload_kind = RTP_PAYLOAD_KIND_H264;
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
	struct RTPPacketInfo pkt_info = {0};
	pkt_info.payloadtype = 96;
	int start_time = get_cur_time();
	bool eof = false;
	char packet[1500];
	while( true )
	{
		memset(buf, 0, sizeof(buf));
		memset(packet, 0, sizeof(packet));
		int read_len = read_packet(file, buf, &eof);
		printf("read_len: %d\n", read_len);
		if( read_len <= 0 )
			break;

		if( read_len > 0 && false == eof )
		{
			// framerate control
			usleep(1000 * 30);
			int time = (get_cur_time() - start_time) * 90;

			encode_stream_pkt(packet, buf, read_len, time, &pkt_info);
		}
		else
		{
			break;
		}
	}
	return 1;
}