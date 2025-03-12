#ifndef RTP_ENCODER_H_
#define RTP_ENCODER_H_

#include <stdint.h>

enum RTPH264Mode
{
	RTPH264_MODE_SINGLE_NAL_MODE,
	RTPH264_MODE_FU_A_MODE
};

struct RTPPacketInfo
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

#define RTP_MAX_PKT_SIZE 1500

#endif