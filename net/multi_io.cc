#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <pthread.h>


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

#if 0
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

#elif 0
     while(1)
	 {
		struct sockaddr_in clientaddr;
		socklen_t len = sizeof(clientaddr);
		int clientfd = accept(sockfd,(struct sockaddr*)&clientaddr,&len);
		printf("sockfd: %d,accepted clientfd: %d\n",sockfd,clientfd);

		pthread_t thid;
		pthread_create(&thid,NULL,client_thread,&clientfd);
	 }
#else   //select
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
					break;
				}
				send(i,buffer,count,0);
				printf("clientfd: %d ",i);
				printf("recv data count: %d content: %s \n",count,buffer);
				
				}
				
			}
		}

	
#endif

	getchar( );
	//close(clientfd);
	
}
