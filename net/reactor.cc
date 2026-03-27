#include <sys/socket.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/time.h>

#define BUFFER_LENGTH 128
#define fd_count 1048576
#define TIME_SUB_MS(tv1,tv2) ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

typedef int (*RCALLBACK)(int fd);

int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);

struct conn_item {
    int fd;
    char rbuffer[BUFFER_LENGTH];
    int ridx;
    char wbuffer[BUFFER_LENGTH];
    int widx; // 待发送数据的起始偏移
    int wlen; // 待发送数据的总长度
    union {
        RCALLBACK accept_callback;
        RCALLBACK recv_callback;
    } recv_t;
    RCALLBACK send_callback;
};

struct conn_item connlist[fd_count] = {0};
struct timeval tv_begin;
int epfd = 0;

int set_event(int fd, int event, int flag) {
    if (flag) {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
        return 1;
    } else {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
        return 0;
    }
}

int accept_cb(int fd) {
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &len);
    if (clientfd < 0) {
        return -1;
    }

    set_event(clientfd, EPOLLIN, 1);

    connlist[clientfd].fd = clientfd;
    memset(connlist[clientfd].wbuffer, 0, BUFFER_LENGTH);
    memset(connlist[clientfd].rbuffer, 0, BUFFER_LENGTH);
    connlist[clientfd].widx = 0;
    connlist[clientfd].ridx = 0;
    connlist[clientfd].wlen = 0;
    connlist[clientfd].recv_t.recv_callback = recv_cb;
    connlist[clientfd].send_callback = send_cb;

    if ((clientfd % 1000) == 999) {
        struct timeval tv_cur;
        gettimeofday(&tv_cur, NULL);
        long cost_time = TIME_SUB_MS(tv_cur, tv_begin);
        memcpy(&tv_begin, &tv_cur, sizeof(struct timeval));
        printf("clientfd: %d, cost time: %ld\n", clientfd, cost_time);
    }

    return clientfd;
}

int recv_cb(int fd) {
    char *buffer = connlist[fd].rbuffer;
    int idx = connlist[fd].ridx;

    int count = recv(fd, buffer + idx, BUFFER_LENGTH - idx, 0);

    if (count <= 0) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        return -1;
    }
    connlist[fd].ridx += count;

    // 准备发送数据：复制到 wbuffer，设置待发送长度和偏移
    memcpy(connlist[fd].wbuffer, connlist[fd].rbuffer, connlist[fd].ridx);
    connlist[fd].wlen = connlist[fd].ridx;
    connlist[fd].widx = 0;

    // 切换监听写事件
    set_event(fd, EPOLLOUT, 0);
    return count;
}

int send_cb(int fd) {
    char *buffer = connlist[fd].wbuffer;
    int idx = connlist[fd].widx;
    int len = connlist[fd].wlen - idx; // 剩余需要发送的长度

    int count = send(fd, buffer + idx, len, 0);

    if (count < 0) {
        // 发送错误处理（此处简化，实际应根据 errno 处理）
        return -1;
    }

    connlist[fd].widx += count;

    // 检查是否发送完毕
    if (connlist[fd].widx >= connlist[fd].wlen) {
        // 发送完毕，重置索引，切换回读事件
        connlist[fd].ridx = 0;
        connlist[fd].widx = 0;
        connlist[fd].wlen = 0;
        set_event(fd, EPOLLIN, 0);
    }
    // 如果没发送完，会继续触发 EPOLLOUT，直到发送完毕

    return count;
}

int init_sever(unsigned short port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(struct sockaddr_in));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);
    if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))) {
        perror("bind");
        return -1;
    }
    listen(sockfd, 10);

    return sockfd;
}

int main() {
    int port_count = 20;
    unsigned short port = 2048;
    epfd = epoll_create(1);
    for (int i = 0; i < port_count; i++) {
        int sockfd = init_sever(port + i);
        connlist[sockfd].fd = sockfd;
        connlist[sockfd].recv_t.accept_callback = accept_cb;
        set_event(sockfd, EPOLLIN, 1);
    }
    gettimeofday(&tv_begin, NULL);
    struct epoll_event events[1024] = {0};

    while (1) {
        int nready = epoll_wait(epfd, events, 1024, -1);
        int i = 0;
        for (i = 0; i < nready; i++) {
            int connfd = events[i].data.fd;
            if (events[i].events & EPOLLIN) {
                int count = connlist[connfd].recv_t.recv_callback(connfd);
				//printf("recv<--clientfd: %d, count: %d, buffer: %s \n",connfd,count,connlist[connfd].rbuffer);
            } else if (events[i].events & EPOLLOUT) {
                int count = connlist[connfd].send_callback(connfd);
				//printf("send-->clientfd: %d, count: %d, buffer: %s \n",connfd,count,connlist[connfd].wbuffer);
            }
        }
    }

    getchar();
}