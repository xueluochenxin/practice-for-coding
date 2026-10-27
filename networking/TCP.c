#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <errno.h>
#include <fcntl.h>

#include <sys/epoll.h>

#define BUFFER_LENGTH		1024*4
#define EPOLL_SIZE			1024
#define LISTEN_QUEUE         5    //全连接队列的最大长度





int main(int argc,char** argv)
{
    if(argc<2)
    {
        fprintf(stderr,"command wrong\n");
        return -1;
    }
    int port=atoi(argv[1]);
    //socket
    int sockfd=socket(AF_INET,SOCK_STREAM,0);  //IP4 tcp 
    
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(struct sockaddr_in));
    addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    //bind
    if(bind(sockfd,(struct sockaddr*)&addr,sizeof(struct sockaddr_in))<0)
    {
       perror("bind");return -2;
    }
    //listen
    if(listen(sockfd,LISTEN_QUEUE)<0)
    {
        perror("listen"); return -3;
    }
    
    //epoll 实现多路复用
    int epollfd=epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event events[EPOLL_SIZE];
    struct epoll_event ev;
    ev.data.fd=sockfd;
    ev.events=EPOLLIN; //只关心什么时候可读
    
    epoll_ctl(epollfd,EPOLL_CTL_ADD,sockfd,&ev);
    while(1)
    {
       //用epoll wait监听，判断哪些文件描述符准备就绪了
       int waitsize=epoll_wait(epollfd,events,EPOLL_SIZE,5);  //本次返回最大参数 epoll_size 5是时间间隔
       if(waitsize<0) continue;
       for(int i=0;i<waitsize;++i)
       {
        if(events[i].data.fd==sockfd) //listen到了新的链接
        {
            struct sockaddr_in client_addr;
				memset(&client_addr, 0, sizeof(struct sockaddr_in));
				socklen_t client_len = sizeof(client_addr);

				int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
                fcntl(clientfd, F_SETFL, O_NONBLOCK);

				ev.events = EPOLLIN | EPOLLET; 
				ev.data.fd = clientfd;   //修改ev，然后加入进epoll
				epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev);
        }
                else //以前的描述符传入了新的数据
        {
            int clientfd = events[i].data.fd;
            
            // 【修改点2】ET 模式必须循环读，直到读完
            while(1) {
                char buffer[BUFFER_LENGTH] = {0};
                int len = recv(clientfd, buffer, BUFFER_LENGTH - 1, 0); // 留一个位置给 \0
                
                if (len > 0) {
                    printf("Recv: %s, %d byte(s)\n", buffer, len);
                } 
                else if (len == 0) { // disconnect 客户端断开
                    printf("clientfd: %d disconnected\n", clientfd);
                    close(clientfd);
                    ev.data.fd = clientfd;
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, &ev);
                    break; // 退出循环
                } 
                else { // len < 0
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 数据读完了，这是正常的，不是错误
                        break; // 跳出循环，等待下一次边沿触发
                    }
                    // 真正的错误
                    perror("recv");
                    close(clientfd);
                    ev.data.fd = clientfd;
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, &ev);
                    break; // 退出循环
                }
            }
        }
       }
    }
    return 0;
}