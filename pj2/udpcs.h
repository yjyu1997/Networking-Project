#ifndef _UDPCS_H
#define _UDPCS_H

#define SER_PORT 1047
#define BUF_SIZE 1020
#define ACK "GET"

struct block
{
	int b_len;
	char b_buf[BUF_SIZE];
};
#endif
