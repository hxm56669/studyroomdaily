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

#if 0  //基础版本
	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);
	int clientfd = accept(sockfd,(struct sockaddr*)&clientaddr,&len);
	printf("sockfd: %d,accepted clientfd: %d\n",sockfd,clientfd);


	#if 0
			char buffer[128] = {0};
			int count = recv(clientfd,buffer,128,0);
			send(clientfd,buffer,count,0);
			printf("recv data content: %s count: %d\n",buffer,count);
	#else
			while(1){
			char buffer[128] = {0};
			int count = recv(clientfd,buffer,128,0);
			if(count == 0){
				break;
			}
			send(clientfd,buffer,count,0);
			printf("recv data content: %s count: %d\n",buffer,count);
			}
		
	#endif

#elif 0  //多线程版本
     while(1)
	 {
		struct sockaddr_in clientaddr;
		socklen_t len = sizeof(clientaddr);
		int clientfd = accept(sockfd,(struct sockaddr*)&clientaddr,&len);
		printf("sockfd: %d,accepted clientfd: %d\n",sockfd,clientfd);

		pthread_t thid;
		pthread_create(&thid,NULL,client_thread,&clientfd);
	 }
#elif 0   //select
    fd_set rdfs,rset;
    FD_ZERO(&rdfs);
	FD_SET(sockfd,&rdfs);
	int maxfd = sockfd;
	while(1)
	{
		rset = rdfs;
     	int nready = select(maxfd+1,&rset,NULL,NULL,NULL);
		if(FD_ISSET(sockfd,&rset)){
			struct sockaddr_in clientaddr;
			socklen_t len = sizeof(clientaddr);
			int clientfd = accept(sockfd,(struct sockaddr*)&clientaddr,&len);
			printf("clientfd: %d\n",clientfd);

			FD_SET(clientfd,&rdfs);
			maxfd = clientfd;
		}
		
		for(int i = sockfd+1;i<=maxfd;i++){
			if(FD_ISSET(i,&rset)){
				char buffer[128] = {0};
				int count = recv(i,buffer,128,0);
				if(count == 0){
					printf("clientfd: %d close\n",i);
					
					FD_CLR(i,&rfds);
					close(i);
					continue;
				}
				send(i,buffer,count,0);
				printf("clientfd: %d ",i);
				printf("recv data count: %d content: %s \n",count,buffer);
				
				}
				
			}
		}
#elif 0 //poll
		struct pollfd fds[1024] = {0};
		memset(fds,-1,sizeof(fds));
		fds[0].fd = sockfd;
		fds[0].events = POLLIN;
		int maxfd = sockfd;

		while(1)
		{
			int nready = poll(fds,1024,-1);
			if(fds[0].revents & POLLIN)
			{
				struct sockaddr_in clientaddr;
				socklen_t len = sizeof(clientaddr);
				int clientfd = accept(sockfd,(struct sockaddr*)&clientaddr,&len);
				printf("clientfd: %d\n",clientfd);

				for (int i = 1; i < 1024; i++) 
				{
					if (fds[i].fd == -1) {
						fds[i].fd = clientfd;
						fds[i].events = POLLIN;
						break;
					}
				}
			}

			for(int i = 1 ; i <1024 ; i++)
			{
				int fd = fds[i].fd;
				if (fd == -1) continue;
				if(fds[i].revents & POLLIN)
				{
					char buffer[128] = {0};
					int count = recv(fd,buffer,128,0);
					if(count <= 0)
					{
						printf("clientfd: %d close\n",i);
						fds[i].fd = -1;
						fds[i].events = 0;
						close(fd);
						
					}else
					{
						send(i,buffer,count,0);
						printf("clientfd: %d ",i);
						printf("recv data count: %d content: %s \n",count,buffer);
					}
					
				}
			}
			
		}
#else  //epoll

		int epfd = epoll_create(1);
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
					struct sockaddr_in clientaddr;
					socklen_t len = sizeof(clientaddr);
					int clientfd = accept(sockfd,(struct sockaddr*)&clientaddr,&len);
					ev.events = EPOLLIN;
					ev.data.fd = clientfd;
					epoll_ctl(epfd,EPOLL_CTL_ADD,clientfd,&ev);
					printf("clientfd: %d\n" ,clientfd);
				}
				else if(events[i].events & EPOLLIN)
				{
					char buffer[128] = {0};
					int count = recv(connfd,buffer,128,0);
					if(count <= 0)
					{
						printf("clientfd: %d close\n",connfd);
						epoll_ctl(epfd,EPOLL_CTL_DEL,connfd,NULL);
						close(connfd);
						
					}else
					{
						send(connfd,buffer,count,0);
						printf("clientfd: %d ",connfd);
						printf("recv data count: %d content: %s \n",count,buffer);
					}
				}
			}
		}

  
	
#endif

	getchar( );
	//close(clientfd);
	
}
