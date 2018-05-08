#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "udpcs.h"
#include "gbn.h"

int main(int argc, char *argv[])
{
    /*
     * argv[1] : recv_host
     * argv[2] : recv_port
     * argv[3] : filename
     */
	int sockfd, fd;
	socklen_t len = sizeof(struct sockaddr_in);
	struct sockaddr_in cli_addr, ser_addr;
	struct block file_blk;

	if ((sockfd = gbnSocket(AF_INET)) == -1)
	{
		printf("socket create error!\n");
		exit(-1);
	}

	bzero(&ser_addr, sizeof(ser_addr));
	ser_addr.sin_family = AF_INET;
	ser_addr.sin_port = htons(SER_PORT);
	ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	bzero(&cli_addr, sizeof(cli_addr));
	cli_addr.sin_family = AF_INET;
	cli_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &cli_addr.sin_addr);

	if (gbnBind(sockfd, (struct sockaddr_in *)&ser_addr, sizeof(struct sockaddr), (struct sockaddr_in *)&cli_addr, &len) == -1)
	{
		printf("socket bind error!\n");
		exit(-1);
	}
	if ((fd = open(argv[3], O_RDONLY, 0444)) == -1)
	{
		printf("Open file error: %s\n", file_blk.b_buf);
		bzero(&file_blk, sizeof(file_blk));
		strncpy(file_blk.b_buf, strerror(errno), BUF_SIZE);
		gbnSend(sockfd, &file_blk, sizeof(file_blk), 0, -1);
	}
	else
	{
		bzero(&file_blk, sizeof(file_blk));
		strcpy(file_blk.b_buf, argv[3]);//send file name
		gbnSend(sockfd, &file_blk, sizeof(file_blk), 0, -1);
		bzero(&file_blk, sizeof(file_blk));
		int offset = 0;
		while((file_blk.b_len = read(fd, file_blk.b_buf, BUF_SIZE)) > 0)
		{
			gbnSend(sockfd, &file_blk, sizeof(file_blk), 0, offset);
			offset += file_blk.b_len;
			bzero(&file_blk, sizeof(file_blk));
		}
        printf("completed!\n\n");
	}
	gbnClose(sockfd);
	return 0;
}
