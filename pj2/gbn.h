#ifndef _GBN_H
#define _GBN_H

#define WIN_SIZE 16
#define GBN_WIN "GBN_WIN"
#define GBN_MUTEX "GBN_MUTEX"
#define GBN_DATA_SIZE 1024
#define GBN_TIMEOUT_USEC 1000
#define GBN_TIMEOUT_SEC 1
#define SEM_NAME_SIZE 20
#define SEM_SUFFIX_SIZE 3

#include <sys/types.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/ipc.h>

struct GbnPacket
{
	int p_seq;
	int p_ack;
	int p_len;
	char p_data[GBN_DATA_SIZE];
	int p_file_offset;
};

struct GbnArg
{
	const void *arg_buf;
	size_t arg_len;
	int arg_flags;
	int file_offset;
};

struct GbnCtl
{
	struct GbnPacket c_win[WIN_SIZE];
	int c_id;
	int c_base;
	int c_next;
	int c_head;
	int c_tail;
	int c_recv_seq;
	int c_packet_num;
	const struct sockaddr_in *c_src_addr;
	struct sockaddr_in *c_dest_addr;
	socklen_t c_src_addrlen;
	socklen_t *c_dest_addrlen;
	int c_sockfd;
	struct GbnArg c_arg;
};

int gbnSocket(int domain);
int gbnBind(int sockfd, const struct sockaddr_in *src_addr, socklen_t s_addrlen, struct sockaddr_in *dest_addr, socklen_t *d_addrlen);
ssize_t gbnRecv(int sockfd, void *buf, size_t len, int flags, int file_offset);
ssize_t gbnSend(int sockfd, const void *buf, size_t len, int flags, int file_offset);
int gbnClose(int sockfd);

#endif
