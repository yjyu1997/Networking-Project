#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "udpcs.h"
#include "gbn.h"

int main(int argc, char *argv[])
{
	int sockfd, fd;
	socklen_t len = sizeof(struct sockaddr_in);
	struct sockaddr_in cli_addr, ser_addr;
	struct block file_blk;
	if (argc != 2)
		return -EINVAL;
	if ((sockfd = gbnSocket(AF_INET)) == -1)
	{
		printf("socket create error!\n");
		exit(-1);
	}

	bzero(&cli_addr, sizeof(cli_addr));
	cli_addr.sin_family = AF_INET;
	cli_addr.sin_port = htons(atoi(argv[1]));
	cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (gbnBind(sockfd, (struct sockaddr_in *)&cli_addr, sizeof(struct sockaddr), (struct sockaddr_in *)&ser_addr, &len) == -1)
	{
		printf("socket bind error!\n");
		exit(-1);
	}

	bzero(&file_blk, sizeof(file_blk));
	gbnRecv(sockfd, &file_blk, sizeof(file_blk), 0, -1);
    if ((fd = open(file_blk.b_buf, O_WRONLY|O_CREAT|O_TRUNC, 0600)) == -1)
        exit(-1);
    int offset = 0;
    while(gbnRecv(sockfd, &file_blk, sizeof(file_blk), 0, offset))
    {
        write(fd, file_blk.b_buf, file_blk.b_len);
        if (file_blk.b_len < BUF_SIZE)
            break;
        offset += file_blk.b_len;
        bzero(&file_blk, sizeof(file_blk));
    }
    close(fd);
    printf("completed!\n");
    fflush(stdout);
	gbnClose(sockfd);
	return 0;
}
