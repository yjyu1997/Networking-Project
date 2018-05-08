#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <netinet/in.h>
#include "common.h"
char buffer[MAX_STR_SIZE];
char buffer1[3][30];

int main(int argc, char **argv)
{
	int sockfd, n;
	struct sockaddr_in servaddr;
	if(argc != 2)
	{
		printf("usage client <c11885685>\n");
		exit(-1);
	}
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("socket error\n");
		exit(-1);
	}
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERVER_PORT);
	if( inet_pton(AF_INET, SERVER_ADDR, &servaddr.sin_addr) <= 0)
	{
		printf("inet_pton error for %s\n", SERVER_ADDR);
		exit(-1);
	}
	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		printf("connect error.\n");
		exit(-1);
	}
	sprintf(buffer, "%s %s %s", MAGIC_STRING, "HELLO", argv[1]);
	write(sockfd, buffer, strlen(buffer) + 1);//send HELLO message
	printf("%s\n", buffer);

	read(sockfd, buffer, MAX_STR_SIZE);	//read STATUS message
	printf("%s\n", buffer);

	int cookie1 = 0, cookie2 = 0;
	sscanf(buffer, "%s%s%d%d%s", buffer1[0], buffer1[1], &cookie1, &cookie2, buffer1[2]);
	sprintf(buffer, "%s %s %d", MAGIC_STRING, "BYE", cookie1 + cookie2);//send BYE message
	write(sockfd, buffer, strlen(buffer) + 1);//send HELLO message
	printf("%s\n", buffer);

	read(sockfd, buffer, MAX_STR_SIZE);	//read CONFIRM_BYE message
	printf("%s\n", buffer);
	return 0;
}
