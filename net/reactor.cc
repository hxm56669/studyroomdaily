#include <sys/socket.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>

#define BUFFER_LENGTH 128

//listenfd  触发 EPOLLIN 时，执行accept_cb
int accept_cb(int fd );
//clientfd 触发 EPOLLIN 时，执行recv_cb;触发 EPOLLOUT 时，执行send_cb

int recv_cb(int fd );
int send_cb( int fd);

struct conn_item
{
	int fd;
	char buffer[BUFFER_LENGTH];
	int idx;
};
struct conn_item connlist[1024] = {0};
int epfd =0;
#if 0
struct reactor
{
	int epfd;
	struct conn_item *connlist;
}
#endif

void* client_thread(void* arg)
{
	int clientfd = *(int*)arg;
	while(1){
	char buffer[128] = {0};
	int count = recv(clientfd,buffer,128,0);
	if(count == 0){
		break;
	}
	send(clientfd,buffer,count,0);
	printf("recv data form clientfd:%d ; count: %d ; content: %s\n",clientfd,count,buffer);
	}
	close(clientfd);
	return NULL;
	
}

int set_event(int fd , int event,int flag)
{
	if(flag)
	{
		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd;
		epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&ev);
	}else{
		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd;
		epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&ev);
	}
	
}
int accept_cb(int fd )
{
	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);
	int clientfd = accept(fd,(struct sockaddr*)&clientaddr,&len);
	if(clientfd < 0)
	{
		return -1;
	}

	set_event(clientfd,EPOLLIN,1);

	connlist[clientfd].fd = clientfd;
	memset(connlist[clientfd].buffer , 0 ,BUFFER_LENGTH);
	connlist[clientfd].idx = 0;

	
	return clientfd;
}

int recv_cb(int fd )
{
	char *buffer = connlist[fd].buffer;
	int idx = connlist[fd].idx;

	int count = recv(fd,buffer+idx,BUFFER_LENGTH-idx,0);
	connlist[fd].idx += count;
	if(count <= 0)
	{
		printf("clientfd: %d close\n",fd);
		epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
		close(fd);
		return -1;
	}
	
	set_event(fd,EPOLLOUT,0);

	return count;
	
}

int send_cb(int fd)
{
	char *buffer = connlist[fd].buffer;
	int idx = connlist[fd].idx;
	
	int count = send(fd,buffer,connlist[fd].idx,0);
	
	set_event(fd,EPOLLIN,0);
	return count;
}


int main( )
{
	int sockfd = socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in serveraddr;
	memset(&serveraddr,0,sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port  = htons(2048);
	if(-1 == bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(struct sockaddr)))
	{
		perror("bind");
		return -1;
	}
	listen(sockfd,10);

	epfd = epoll_create(1);
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = sockfd;
	epoll_ctl(epfd,EPOLL_CTL_ADD,sockfd,&ev);

	struct epoll_event events[1024] = {0};

	while(1)
	{
		int nready = epoll_wait(epfd,events,1024,-1);
		int i = 0;
		for(i = 0; i < nready ; i++)
		{
			int connfd = events[i].data.fd;
			if(sockfd == connfd)
			{
				int clientfd = accept_cb(sockfd);
				printf("clientfd: %d\n" ,clientfd);
			}
			else if(events[i].events & EPOLLIN)
			{
				int count = recv_cb(connfd);
				printf("recv<--clientfd: %d, count: %d, buffer: %s \n",connfd,count,connlist[connfd].buffer);
			}
			else if(events[i].events & EPOLLOUT)
			{
				int count = send_cb(connfd);
				printf("send-->clientfd: %d, count: %d, buffer: %s \n",connfd,count,connlist[connfd].buffer);
			}
		}
	}

  
	


	getchar( );
	//close(clientfd);
	
}
