#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit, malloc
#include <string.h>     // memset, bzero
#include <unistd.h>     // close, read, write
#include <errno.h>      // errno
#include <fcntl.h>      // fcntl (如果要用非阻塞 ET 模式)
#include <sys/socket.h> // socket, bind, listen, accept, recv, send
#include <netinet/in.h> // sockaddr_in, htons, htonl, INADDR_ANY
#include <arpa/inet.h>  // inet_ntop, inet_addr (打印客户端 IP)
#include <sys/epoll.h>  // epoll_create1, epoll_ctl, epoll_wait, struct epoll_event

#define BUFFER_SIZE 1024
#define CONN_SIZE 65536
#define ADD_FLAG 1
#define MOD_FLAG 0
#define MAX_EVENTS 1024

typedef int (*CALLBACK)(int fd);

int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);
int set_nonblocking(int fd);
int set_event(int fd, int event, int flag);

struct conn
{
    int fd;
    char rbuffer[BUFFER_SIZE];
    int rlen;
    char wbuffer[BUFFER_SIZE];
    int wlen;

    CALLBACK send_cb;
    union
    {
        CALLBACK recv_cb;
        CALLBACK accept_cb;
    } r_action;
};

int epfd = -1;
struct conn conn_list[CONN_SIZE] = {0};

int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int set_event(int fd, int event, int flag) // 给定fd和event 加入epoll检测目标当中.
{
    struct epoll_event ev;
    ev.events = event;
    ev.data.fd = fd;
    if (flag)
    {
        return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    }
    else
    {
        return epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    }
}

int init_server(unsigned short port) // socket bind listen
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }
    struct sockaddr_in server_add;
    memset(&server_add, 0, sizeof(struct sockaddr_in));
    server_add.sin_family = AF_INET;
    server_add.sin_addr.s_addr = htonl(INADDR_ANY);
    server_add.sin_port = htons(port);

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // 端口复用
    if (bind(sockfd, (struct sockaddr *)&server_add, sizeof(struct sockaddr_in)))
    {
        close(sockfd);
        perror("bind");
        return -1;
    }

    if (listen(sockfd, 1024) < 0)
    {
        perror("listen");
        close(sockfd);
        return -1;
    }
    set_nonblocking(sockfd);
    return sockfd;
}

int accept_cb(int fd)
{
    while (1)
    {
        struct sockaddr_in addr;
        socklen_t socklen = sizeof(struct sockaddr_in);
        int clientfd = accept(fd, (struct sockaddr *)&addr, &socklen);
        if (clientfd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; // 没更多连接
            if (errno == EINTR)
                continue;
            perror("accept");
            return -1; // ★ 关键：失败必须 return
        }
        // 把新的fd加入ev监控列表
        set_nonblocking(clientfd);
        conn_list[clientfd].fd = clientfd;
        conn_list[clientfd].r_action.recv_cb = recv_cb;
        conn_list[clientfd].send_cb = send_cb;
        memset(conn_list[clientfd].rbuffer, 0, BUFFER_SIZE);
        conn_list[clientfd].rlen = 0;
        memset(conn_list[clientfd].wbuffer, 0, BUFFER_SIZE);
        conn_list[clientfd].wlen = 0;
        set_event(clientfd, EPOLLIN, ADD_FLAG);
    }
    return 0;
}

int recv_cb(int fd)
{
    memset(conn_list[fd].rbuffer, 0, BUFFER_SIZE);
    int count = recv(fd, conn_list[fd].rbuffer, BUFFER_SIZE - 1, 0);
    if (count == 0) // 断开连接
    {
        printf("client disconnect:%d \n", fd);
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        return -1;
    }
    if (count < 0) // 出错
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        {
            // 读完了，正常结束
            return 0;
        }
        else // 出错了
        {
            perror("recv");
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            return -1;
        }
    }
    // recv不会自动加入\0  需要我们手动加进去
    conn_list[fd].rbuffer[count] = '\0';
    conn_list[fd].rlen = count;
    return count;
}

int send_cb(int fd)
{
}

int main_loop(struct epoll_event *Events, int sockfd)
{
    while (1)
    {
        int nready = epoll_wait(epfd, Events, MAX_EVENTS, -1);
        if (nready == -1)
        {
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < nready; ++i)
        {
            if (Events[i].data.fd == sockfd)
            {
                accept_cb(Events[i].data.fd);
            }
            else
            {
                if (Events[i].events & (EPOLLERR | EPOLLHUP)) //出错  挂断
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, Events[i].data.fd, NULL);
                    close(Events[i].data.fd);
                    continue; // ★ 处理完直接跳过，不要再走 IN/OUTS
                }
                if (Events[i].events & EPOLLIN)
                {
                    if (recv_cb(Events[i].data.fd) < 0) continue;   // ★ 已关闭，跳过 OUT
                }
                if (Events[i].events & EPOLLOUT)
                {
                    send_cb(Events[i].data.fd);
                }
            }
        }
    }
    return 0;
}
int main()
{
    epfd = epoll_create1(0);

    struct epoll_event Events[MAX_EVENTS];
    memset(Events, 0, sizeof(struct epoll_event) * MAX_EVENTS);

    unsigned short port = 2000;
    int sockfd = init_server(port);
    conn_list[sockfd].fd = sockfd;
    conn_list[sockfd].r_action.accept_cb = accept_cb;
    set_event(sockfd, EPOLLIN, ADD_FLAG);

    main_loop(Events, sockfd);
    close(epfd);
    close(sockfd);
    return 0;
}