#include <stdio.h>          // printf, perror
#include <stdlib.h>         // exit, malloc
#include <string.h>         // memset, bzero
#include <unistd.h>         // close, read, write
#include <errno.h>          // errno
#include <fcntl.h>          // fcntl (如果要用非阻塞 ET 模式)
#include <sys/socket.h>     // socket, bind, listen, accept, recv, send
#include <netinet/in.h>     // sockaddr_in, htons, htonl, INADDR_ANY
#include <arpa/inet.h>      // inet_ntop, inet_addr (打印客户端 IP)
#include <sys/epoll.h>      // epoll_create1, epoll_ctl, epoll_wait, struct epoll_event

#define MAX_EVENTS 1024
#define BUF_SIZE 2048 
int set_nonblocking(int fd);

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}



int main()
{
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in server_add;
    memset(&server_add,0,sizeof(struct sockaddr_in));
    server_add.sin_family=AF_INET;
    server_add.sin_addr.s_addr=htonl(INADDR_ANY);
    server_add.sin_port=htons(2000);

    if(bind(sockfd,(struct sockaddr*)&server_add,sizeof(struct sockaddr_in)))
    {
      close(sockfd);
      perror("bind");
      return -1;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));//

    if (listen(sockfd, 1024) < 0) 
    {
    perror("listen");
    close(sockfd);
    return -1;
    }
    

    //epoll
    int epfd=epoll_create1(0);
    if(epfd<0)
    {
        perror("epoll_create1");
        return -1;
    }
    struct epoll_event ev;
    ev.events=EPOLLIN|EPOLLET;  //边沿触发只读
    ev.data.fd=sockfd;

    set_nonblocking(sockfd);
    epoll_ctl(epfd,EPOLL_CTL_ADD,sockfd,&ev); //把sockfd加入epoll要检测的队列当中
    
    struct epoll_event events[MAX_EVENTS];
    while(1) 
    {
        int nready=epoll_wait(epfd,events,MAX_EVENTS,-1);
        if (nready == -1) {
            perror("epoll_wait");
            break;
        }

        for(int i=0;i<nready;++i)//此时我们遍历events，可能是有新连接或者是有新数据
        {
            int fd=events[i].data.fd;
            if(fd==sockfd)//有新的链接需要accept了
            {
                while(1)
                {
                    struct sockaddr_in addr;
                    socklen_t socklen=sizeof(struct sockaddr_in);
                    int clientfd=accept(fd,(struct sockaddr*)&addr,&socklen);
                    if(clientfd==-1&&(errno == EAGAIN || errno == EWOULDBLOCK))
                    {
                        //fprintf(stderr,"accept finished\n");
                        break;
                    }
                    //把新的fd加入ev监控列表
                    set_nonblocking(clientfd);
                    ev.data.fd=clientfd;
                    ev.events= EPOLLIN | EPOLLET; // 初始只监听读事件,边沿触发
                    epoll_ctl(epfd,EPOLL_CTL_ADD,clientfd,&ev);
                }
            }
            else//有链接断开了需要close，或者有新的数据需要读取
            {
                if(events[i].events&EPOLLIN)//有数据需要读
                {
                    char buffer[BUF_SIZE];
                    while(1)
                    {
                        
                        int count=recv(fd,buffer,BUF_SIZE,0);
                        if(count>0)
                        {
                            //recv不会自动加\0需要手动加
                            buffer[count]='\0';
                            printf("recv: %s    \n",buffer);


                        }
                        else if(count==0)//主动断开连接了
                        {
                            epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
                            close(fd);break;
                        }
                        else//count <0
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                // ET 模式下，数据读干了，正常退出循环
                                break; 
                            }
                            // 发生其他错误（如 ECONNRESET），也需要关闭连接
                            perror("recv error");
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                            close(fd);
                            break;
                        }
                    }



                }
                else if(events[i].events&EPOLLOUT)//有东西需要写
                {

                }
            }
        }


    }


    close(epfd);
    close(sockfd);
    return 0;
}