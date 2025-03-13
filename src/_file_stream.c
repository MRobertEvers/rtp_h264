#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

	if( offset > 0 )
	{
		// We still have remaining data in buf_read, parse it
		parse_len = parse_nal(buf_read, offset, &startcode_len);
		if( parse_len > startcode_len )
		{
			memcpy(buf, buf_read + startcode_len, parse_len - startcode_len);
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
			parse_len = parse_nal(buf_read, offset + read_len, &startcode_len);
			if( parse_len > startcode_len )
			{
				memcpy(
					buf, buf_read + startcode_len, parse_len - startcode_len);
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

	return parse_len - startcode_len;

finished:
	*eof = true;
	memset(buf, 0, sizeof(buf));
	return 0;
}