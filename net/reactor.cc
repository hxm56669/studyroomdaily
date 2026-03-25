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

typedef int (*RCALLBACK)(int fd);
//listenfd  触发 EPOLLIN 时，执行accept_cb
int accept_cb(int fd );
//clientfd 触发 EPOLLIN 时，执行recv_cb;触发 EPOLLOUT 时，执行send_cb

int recv_cb(int fd );
int send_cb( int fd);

struct conn_item
{
	int fd;
	char rbuffer[BUFFER_LENGTH];
	int ridx;
	char wbuffer[BUFFER_LENGTH];
	int widx;
	union 
	{
		RCALLBACK accept_callback;
		RCALLBACK recv_callback;	
	}recv_t;
	RCALLBACK send_callback;
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
		return 1;
	}else{
		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd;
		epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&ev);
		return 0;
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
	memset(connlist[clientfd].wbuffer , 0 ,BUFFER_LENGTH);
	memset(connlist[clientfd].rbuffer , 0 ,BUFFER_LENGTH);
	connlist[clientfd].widx = 0;
	connlist[clientfd].ridx = 0;
	connlist[clientfd].recv_t.recv_callback = recv_cb;
	connlist[clientfd].send_callback = send_cb;

	
	return clientfd;
}

int recv_cb(int fd )
{
	char *buffer = connlist[fd].rbuffer;
	int idx = connlist[fd].ridx;

	int count = recv(fd,buffer+idx,BUFFER_LENGTH-idx,0);
	
	if(count <= 0)
	{
		printf("clientfd: %d close\n",fd);
		epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
		close(fd);
		return -1;
	}
	connlist[fd].ridx += count;
#if 1
	memcpy(connlist[fd].wbuffer,buffer,connlist[fd].ridx);
	
#endif
	set_event(fd,EPOLLOUT,0);
	return count;
	
}

int send_cb(int fd)
{
	char *buffer = connlist[fd].wbuffer;
	int idx = connlist[fd].widx;
	
	int count = send(fd,buffer+idx,connlist[fd].widx,0);
	connlist[fd].widx = connlist[fd].ridx;
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
	connlist[sockfd].fd = sockfd;
	connlist[sockfd].recv_t.accept_callback = accept_cb;
	

	epfd = epoll_create(1);

	set_event(sockfd,EPOLLIN,1);
	

	struct epoll_event events[1024] = {0};

	while(1)
	{
		int nready = epoll_wait(epfd,events,1024,-1);
		int i = 0;
		for(i = 0; i < nready ; i++)
		{
			int connfd = events[i].data.fd;
			/*if(sockfd == connfd)
			{
				int clientfd = connlist[sockfd].accept_callback(sockfd);
				printf("clientfd: %d\n" ,clientfd);
			}
			else */
			if(events[i].events & EPOLLIN)
			{
				int count = connlist[connfd].recv_t.recv_callback(connfd);
				printf("recv<--clientfd: %d, count: %d, buffer: %s \n",connfd,count,connlist[connfd].rbuffer);
			}
			else if(events[i].events & EPOLLOUT)
			{
				int count = connlist[connfd].send_callback(connfd);
				printf("send-->clientfd: %d, count: %d, buffer: %s \n",connfd,count,connlist[connfd].wbuffer);
			}
		}
	}

  
	


	getchar( );
	//close(clientfd);
	
}
