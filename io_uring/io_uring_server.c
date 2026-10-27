//异步 两者之间的关系
/*
*/
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
#include <liburing.h>


#define ENTRIES_LENGTH 1024
#define EVENT_ACCEPT 0
#define EVENT_READ  1
#define EVENT_WRITE 2
#define BUFFER_SIZE 1024

struct conn_info{
int fd;int event;
char *buf;
};

int set_event_send(struct io_uring *ring,int sockfd, char* buf,
                  size_t len, int flags);
int set_nonblocking(int fd);
int init_server(unsigned short port); 
int set_event_accept(struct io_uring *ring,int sockfd, struct sockaddr *addr,
                  socklen_t * addrlen,int flags);           
int set_event_recv(struct io_uring *ring,int sockfd, char* buf,
                  size_t len,int flags);



int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
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
int set_event_accept(struct io_uring *ring,int sockfd, struct sockaddr *addr,
                  socklen_t * addrlen,int flags)
{    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) { //防止 SQ 队列满
        fprintf(stderr, "SQE 获取失败，队列已满\n");
        return -1;
    }

    struct conn_info *info = malloc(sizeof(struct conn_info));
    info->fd = sockfd;
    info->event = EVENT_ACCEPT;
    info->buf = NULL;

    io_uring_prep_accept(sqe, sockfd, addr, addrlen, flags);

    io_uring_sqe_set_data(sqe, info);

}
int set_event_recv(struct io_uring *ring,int sockfd, char* buf,
                  size_t len,int flags)
{
    struct io_uring_sqe* sqe=io_uring_get_sqe(ring);
    if(!sqe)
    {
        return -1;
    }
    struct conn_info *info = malloc(sizeof(struct conn_info));
    info->fd = sockfd;
    info->event = EVENT_READ;
    info->buf = buf;

    io_uring_prep_recv(sqe, sockfd, buf, len, flags);

    io_uring_sqe_set_data(sqe, info);
}
int set_event_send(struct io_uring *ring,int sockfd, char* buf,
                  size_t len, int flags)
{
    struct io_uring_sqe* sqe=io_uring_get_sqe(ring);
    if(!sqe)
    {
        return -1;
    }
    struct conn_info *info = malloc(sizeof(struct conn_info));
    info->fd = sockfd;
    info->event = EVENT_WRITE;
    info->buf = buf;

    io_uring_prep_send(sqe, sockfd, buf, len, flags);

    io_uring_sqe_set_data(sqe, info);

}
int main()
{
    unsigned short port=9999;
    int sockfd=init_server(port);

    //
    struct io_uring_params params;
    memset(&params,0,sizeof(struct io_uring_params));

    struct io_uring ring;
    io_uring_queue_init_params(ENTRIES_LENGTH,&ring,&params);

    struct io_uring_sqe* sqe;
    sqe=io_uring_get_sqe(&ring);
    //

    struct sockaddr_in client_addr;
    socklen_t lensock=sizeof(struct sockaddr_in);
    // struct conn_info accept_info;
    // accept_info.event=EVENT_ACCEPT;
    // accept_info.fd=sockfd;
    // accept_info.buf=NULL;

    set_event_accept(&ring,sockfd,(struct sockaddr*)&client_addr,&lensock,0);
    struct io_uring_cqe *cqes[128];

    while(1)
    {
        int ret = io_uring_submit_and_wait(&ring, 1);  //至少等一个事件


        if (ret < 0) {
            perror("io_uring_submit_and_wait");
            break;
        }


        struct io_uring_cqe *cqe;
        //io_uring_wait_cqe(&ring,&cqe);
        
        int nready= io_uring_peek_batch_cqe(&ring,cqes,128);
        for(int i=0;i<nready;++i)
        {
            struct io_uring_cqe* cqe=cqes[i];
            struct conn_info *result = (struct conn_info *)io_uring_cqe_get_data(cqe);
            if(result==NULL)
            {
                continue;
            }

            if(result->event==EVENT_ACCEPT)
            {
                int ret=cqe->res;
                if(ret>0){
                set_event_accept(&ring,sockfd,(struct sockaddr*)&client_addr,&lensock,0);
                char* currbuf=malloc(BUFFER_SIZE);
                set_event_recv(&ring,ret,currbuf,BUFFER_SIZE,0);
                printf("accept finished\n");

                }
                else
                {
                    set_event_accept(&ring,sockfd,(struct sockaddr*)&client_addr,&lensock,0);
                }
                free(result);
            }
            else if(result->event==EVENT_READ)
            {
                int ret=cqe->res;
                if(ret>0)
                {
                    set_event_send(&ring,result->fd,result->buf,ret,0);
                    printf("read : %s\n",result->buf);
                    free(result);
                }
                else   ////disconnet  || fail
                {
                    printf("disconneted now\n");
                    close(result->fd);
                    free(result->buf);
                    free(result);                    
                }
            }
            else if(result->event==EVENT_WRITE)
            {
                if(cqe->res>0)
                {
                    set_event_recv(&ring,result->fd,result->buf,BUFFER_SIZE,0);
                }
                else
                {
                    close(result->fd);
                    free(result->buf);
                    free(result);
                }
            }
        }
        io_uring_cq_advance(&ring,nready);
    }
    io_uring_queue_exit(&ring);
    close(sockfd);
    return 0;
}