#include <semaphore.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "gbn.h"

void do_send(struct GbnArg *p_arg);
void do_resend();
void do_recvack(struct GbnArg *p_arg);
void do_sendack(int sockfd, int seq, int flags, struct sockaddr_in *addr, socklen_t addrlen);
void init_sigaction();
void start_timer();
void stop_timer();
void restart_timer();
void itoa(int i, char* string, int len);

sem_t *gbn_win = NULL;
sem_t *gbn_mutex = NULL;
struct GbnCtl *ctl = NULL;

int gbnSocket(int domain)
{
	return socket(domain, SOCK_DGRAM, 0);
}

int gbnBind(int sockfd, const struct sockaddr_in *src_addr, socklen_t s_addrlen, struct sockaddr_in *dest_addr, socklen_t *d_addrlen)
{
	char sem_name[SEM_NAME_SIZE];
	time_t seed = time(NULL);
	srand((int)seed);
	int id = rand();
	int shmid = shmget(id, sizeof(struct GbnCtl), IPC_CREAT|0644);
	if (shmid == -1)
		return -1;
	if ((ctl = shmat(shmid, 0, 0)) == (void *)-1)
		return -1;
	bzero(ctl, sizeof(struct GbnCtl));
	ctl->c_id = id;
	ctl->c_dest_addr = dest_addr;
	ctl->c_src_addr = src_addr;
	ctl->c_dest_addrlen = d_addrlen;
	ctl->c_src_addrlen = s_addrlen;
	ctl->c_sockfd = sockfd;

	bzero(sem_name, SEM_NAME_SIZE);
	strncpy(sem_name, GBN_WIN, SEM_NAME_SIZE);
	itoa(id, sem_name + strlen(GBN_WIN), SEM_SUFFIX_SIZE);
	sem_unlink(sem_name);
	gbn_win = sem_open(sem_name, O_CREAT, 0644, WIN_SIZE);

	bzero(sem_name, SEM_NAME_SIZE);
	strncpy(sem_name, GBN_MUTEX, SEM_NAME_SIZE);
	itoa(id, sem_name + strlen(GBN_MUTEX), SEM_SUFFIX_SIZE);
	sem_unlink(sem_name);
	gbn_mutex = sem_open(sem_name, O_CREAT, 0644, 1);
	return bind(sockfd, (struct sockaddr *)src_addr, s_addrlen);
}

ssize_t gbnRecv(int sockfd, void *buf, size_t len, int flags, int file_offset)
{
	struct GbnPacket recv_packet;
	int offset = 0;
	while (len > 0)
	{
		sem_wait(gbn_mutex);
		if(recvfrom(sockfd, &recv_packet, sizeof(recv_packet), flags|MSG_DONTWAIT, (struct sockaddr *)ctl->c_dest_addr, ctl->c_dest_addrlen) != -1)
		{
			if (recv_packet.p_ack == 0 && recv_packet.p_seq == ctl->c_recv_seq)
			{
				memcpy(buf+offset, recv_packet.p_data, recv_packet.p_len);
				offset += recv_packet.p_len;
				len -= recv_packet.p_len;
				ctl->c_recv_seq++;
				if(file_offset >= 0)
                {
                    printf("[recieve data] %d (%d) ACCEPTED\n", file_offset, recv_packet.p_len);
                    file_offset += recv_packet.p_len;
                }
			}
			else
            {
                if(file_offset >= 0)
                {
                    printf("[recieve data] %d (%d) IGNORED\n", file_offset, recv_packet.p_len);
                    file_offset += recv_packet.p_len;
                }
            }
			do_sendack(sockfd, ctl->c_recv_seq, flags, ctl->c_dest_addr, *ctl->c_dest_addrlen);


		}
		sem_post(gbn_mutex);
	}
	fflush(stdout);
	return offset;
}

ssize_t gbnSend(int sockfd, const void *buf, size_t len, int flags, int fileOffset)
{
	pthread_t p_send;
	pthread_t p_recvack;
	struct GbnArg p_arg;
	p_arg.arg_flags = flags;
	p_arg.arg_len = len;
	p_arg.arg_buf = buf;
	p_arg.file_offset = fileOffset;
	init_sigaction();
	start_timer();
	if(pthread_create(&p_send, NULL, (void *)do_send, &p_arg))
		return -1;
	if(pthread_create(&p_recvack, NULL, (void *)do_recvack, &p_arg))
		return -1;
	pthread_join(p_send, NULL);
	pthread_join(p_recvack, NULL);
	stop_timer();
	return len;
}

int gbnClose(int sockfd)
{
	int id = ctl->c_id;
	int shmid = shmget(id, 0, 0644);
	char sem_name[SEM_NAME_SIZE];
	if (shmid == -1)
		return -1;
	shmdt(ctl);
	shmctl(shmid, IPC_RMID, 0);
	bzero(sem_name, SEM_NAME_SIZE);
	strncpy(sem_name, GBN_WIN, SEM_NAME_SIZE);
	itoa(id, sem_name + strlen(GBN_WIN), SEM_SUFFIX_SIZE);
	sem_unlink(sem_name);
	gbn_win = sem_open(sem_name, O_CREAT, 0644, WIN_SIZE);

	bzero(sem_name, SEM_NAME_SIZE);
	strncpy(sem_name, GBN_MUTEX, SEM_NAME_SIZE);
	itoa(id, sem_name + strlen(GBN_MUTEX), SEM_SUFFIX_SIZE);
	sem_unlink(sem_name);
	gbn_mutex = sem_open(sem_name, O_CREAT, 0644, 1);
	return close(sockfd);
}

void do_send(struct GbnArg *p_arg)
{
	struct GbnPacket send_packet;
	int len_t = p_arg->arg_len;
	int offset = 0;
	int sockfd = ctl->c_sockfd;
	const struct sockaddr_in *dest_addr = ctl->c_dest_addr;
	socklen_t addrlen = *ctl->c_dest_addrlen;
	const char* buf = p_arg->arg_buf;
	ctl->c_packet_num = len_t/GBN_DATA_SIZE + (len_t%GBN_DATA_SIZE ? 1 : 0);
	int fileOffset = p_arg->file_offset;

	while (len_t > 0)
	{
		bzero(&send_packet, sizeof(send_packet));
		sem_wait(gbn_win);
		sem_wait(gbn_mutex);
		if (len_t >= GBN_DATA_SIZE)
			send_packet.p_len = GBN_DATA_SIZE;
		else
			send_packet.p_len = len_t;
		memcpy(send_packet.p_data, buf+offset, send_packet.p_len);
		offset += send_packet.p_len;
		len_t -= send_packet.p_len;
		send_packet.p_seq = ctl->c_next;
		send_packet.p_ack = 0;
		send_packet.p_file_offset = fileOffset;
		ctl->c_win[ctl->c_tail] = send_packet;
		ctl->c_next++;
		ctl->c_tail++;
		ctl->c_tail %= WIN_SIZE;
		sendto(sockfd, &send_packet, sizeof(send_packet), p_arg->arg_flags, (struct sockaddr *)dest_addr, addrlen);
		if(fileOffset >= 0)
        printf("[send data] %d (%d)\n", fileOffset, send_packet.p_len);
        fileOffset += send_packet.p_len;
		sem_post(gbn_mutex);
	}
	fflush(stdout);
	pthread_exit(NULL);
}

void do_resend()
{
	int sockfd = ctl->c_sockfd;
	struct sockaddr_in *dest_addr = ctl->c_dest_addr;
	socklen_t addrlen = *ctl->c_dest_addrlen;
	int i = 0;
	int base = ctl->c_head;
	for (i = 0; i < ctl->c_packet_num; i++)
	{
		sendto(sockfd, &ctl->c_win[base], sizeof(struct GbnPacket), 0, (struct sockaddr *)dest_addr, addrlen);
		if(ctl->c_win[base].p_file_offset>=0)
        printf("[resend data] %d (%d)!\n", ctl->c_win[base].p_file_offset,ctl->c_win[base].p_len);
		base++;
		base %= WIN_SIZE;
	}
	fflush(stdout);
}

void do_sendack(int sockfd, int seq, int flags, struct sockaddr_in *addr, socklen_t addrlen)
{
	struct GbnPacket ack_packet;
	bzero(&ack_packet, sizeof(ack_packet));
	ack_packet.p_seq = seq;
	ack_packet.p_ack = 1;
	sendto(sockfd, &ack_packet, sizeof(ack_packet), flags, (struct sockaddr *)addr, addrlen);
	fflush(stdout);
}

void do_recvack(struct GbnArg *p_arg)
{
	struct GbnPacket recv_packet;
	int flags = p_arg->arg_flags;
	int ack_t, i;
	sem_wait(gbn_mutex);
	int sockfd = ctl->c_sockfd;
	struct sockaddr_in *dest_addr = ctl->c_dest_addr;
	socklen_t *addrlen = ctl->c_dest_addrlen;
	sem_post(gbn_mutex);

	for(;;)
	{
		sem_wait(gbn_mutex);
		if(recvfrom(sockfd, &recv_packet, sizeof(recv_packet), flags|MSG_DONTWAIT, (struct sockaddr *)dest_addr, addrlen) == -1)
		{
			sem_post(gbn_mutex);
			continue;
		}
		if (recv_packet.p_ack == 1 && (ack_t = recv_packet.p_seq - ctl->c_base) > 0)
		{
			ctl->c_base = recv_packet.p_seq;
			for (i = 0; i < ack_t; i++)
			{
				sem_post(gbn_win);
				ctl->c_head++;
			}
			ctl->c_head %= WIN_SIZE;
			ctl->c_packet_num--;
            printf("[recieve ack] %d!\n", recv_packet.p_seq);
			restart_timer();
		}
		if (!ctl->c_packet_num)
		{
			sem_post(gbn_mutex);
			break;
		}
		sem_post(gbn_mutex);
	}
	fflush(stdout);
	pthread_exit(NULL);
}

void init_sigaction()
{
	struct sigaction act;
	act.sa_handler = do_resend;
	act.sa_flags = SA_SIGINFO;
	sigemptyset(&act.sa_mask);
	sigaction(SIGPROF, &act, NULL);
}

void start_timer()
{
	struct itimerval value;
	value.it_value.tv_sec = GBN_TIMEOUT_SEC;
	value.it_value.tv_usec = GBN_TIMEOUT_USEC;
	value.it_interval = value.it_value;
	setitimer(ITIMER_PROF, &value, NULL);
	fflush(stdout);
}

void stop_timer()
{
	struct itimerval value;
	value.it_value.tv_sec = 0;
	value.it_value.tv_usec = 0;
	value.it_interval = value.it_value;
	setitimer(ITIMER_PROF, &value, NULL);
	fflush(stdout);
}

void restart_timer()
{
	fflush(stdout);
	stop_timer();
	start_timer();
}

void itoa(int i, char* string, int len)
{
	int power, j;
	j = i;
	for(power = 1; j >= 10; j /= 10)
		power *= 10;
	for(; power && len; power /= 10)
	{
		*(string++) = '0' + i/power;
		i %= power;
		len--;
	}
	*string = '\0';
}
