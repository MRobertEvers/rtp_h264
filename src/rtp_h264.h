#ifndef RTP_ENCODER_H_
#define RTP_ENCODER_H_

#include <stdint.h>

enum RTPH264Mode
{
	RTPH264_MODE_SINGLE_NAL_MODE,
	RTPH264_MODE_FU_A_MODE
};

struct RTPStreamInfo
{
	uint32_t payloadtype;
	uint32_t ssrc;
	uint16_t sequenceno;
};

enum RTPPayloadKind
{
	RTP_PAYLOAD_KIND_MPEG2_TS = 33,
	RTP_PAYLOAD_KIND_H264 = 96
};

struct RTPH264Packetization
{
	uint8_t* data;
	uint32_t len;

	uint32_t _offset;

	enum RTPH264Mode _mode;
	uint8_t _cu_pack_num;
	uint8_t _fu_pack_num;
};

enum RTPH264PacketizationResult
{
	RTP_H264_PACKETIZATION_RESULT_DATA,
	RTP_H264_PACKETIZATION_RESULT_DONE,
	RTP_H264_PACKETIZATION_RESULT_ERROR
};

void rtp_h264_stream_init(
	struct RTPStreamInfo* stream,
	enum RTPPayloadKind payload_kind,
	uint32_t ssrc);

/**
 * H264 data
 */
void rtp_h264_packetize_begin(
	struct RTPStreamInfo* stream,
	struct RTPH264Packetization* packetization,
	uint32_t mtu,
	uint8_t* data,
	uint32_t len);

/**
 * len should be the MTU size
 */
int32_t rtp_h264_packetize_next(
	struct RTPStreamInfo* stream,
	struct RTPH264Packetization* packetization,
	uint8_t* packet,
	uint32_t time);

#endif