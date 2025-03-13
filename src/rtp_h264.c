#include "rtp_h264.h"

#include <stdio.h>
#include <string.h>

#define RTP_HDR_SIZE 12
#define RTP_H264_MAX_NAL_DATA_SIZE(pkt_size) (pkt_size - RTP_HDR_SIZE)

typedef struct nalu_header
{
	uint8_t type : 5;
	uint8_t nri : 2;
	uint8_t f : 1;
} __attribute__((packed)) nalu_header_t;

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

static int
rtp_encode(
	uint8_t* rtp_packet,
	uint32_t rtp_packet_size,
	uint8_t* data,
	int len,
	int time,
	enum RTPH264Mode mode,
	uint8_t cu_pack_num,
	uint8_t fu_pack_num,
	struct RTPStreamInfo* pkt_info)
{
	uint8_t marker_bit = 0;
	uint32_t last_pkt_size = 0;
	uint32_t pkt_data_size = RTP_H264_MAX_NAL_DATA_SIZE(rtp_packet_size);
	nalu_header_t* nalu_hdr;
	fu_header_t* fu_hdr;
	fu_indicator_t* fu_ind;

	if( !data || !len )
	{
		printf("Encode Invalid params or no Data to encode");
		return -1;
	}

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
			memcpy(rtp_packet + index, data + 1, pkt_data_size - 1);
			return index + pkt_data_size - 1;
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
				data + cu_pack_num * pkt_data_size,
				pkt_data_size);
			return index + pkt_data_size;
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
			last_pkt_size =
				len % pkt_data_size ? len % pkt_data_size : pkt_data_size;
			memcpy(
				rtp_packet + index,
				data + cu_pack_num * pkt_data_size,
				last_pkt_size);
			return index + last_pkt_size;
		}
	}
}

void
rtp_h264_stream_init(
	struct RTPStreamInfo* pkt_info,
	enum RTPPayloadKind payload_kind,
	uint32_t ssrc)
{
	pkt_info->payloadtype = payload_kind;
	pkt_info->ssrc = ssrc;
	pkt_info->sequenceno = 0;
}

/**
 * H264 data
 */
void
rtp_h264_packetize_begin(
	struct RTPStreamInfo* stream,
	struct RTPH264Packetization* packetization,
	uint32_t mtu,
	uint8_t* data,
	uint32_t len)
{
	packetization->data = data;
	packetization->len = len;
	packetization->_offset = 0;
	packetization->_mode = RTPH264_MODE_FU_A_MODE;
	packetization->_cu_pack_num = 0;

	uint32_t data_packet_size = RTP_H264_MAX_NAL_DATA_SIZE(mtu);

	if( len < data_packet_size )
		packetization->_mode = RTPH264_MODE_SINGLE_NAL_MODE;
	else
	{
		packetization->_mode = RTPH264_MODE_FU_A_MODE;
		packetization->_cu_pack_num = 0;
		packetization->_fu_pack_num = len % data_packet_size
										  ? (len / data_packet_size + 1)
										  : len / data_packet_size;
	}
}

/**
 * len should be the MTU size
 */
int32_t
rtp_h264_packetize_next(
	struct RTPStreamInfo* stream,
	struct RTPH264Packetization* packetization,
	uint8_t* packet,
	uint32_t time)
{
	uint32_t pkt_size = 0;
	uint8_t* data = packetization->data;
	uint32_t data_len = packetization->len;
	if( packetization->_cu_pack_num == packetization->_fu_pack_num )
		return 0;

	pkt_size = rtp_encode(
		packet,
		packetization->len,
		data,
		data_len,
		time,
		packetization->_mode,
		packetization->_cu_pack_num,
		packetization->_fu_pack_num,
		stream);

	packetization->_cu_pack_num++;
	stream->sequenceno += 1;

	return pkt_size;
}
