#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MESAGE "ABCDEFGHIJKLMNOPQRSTUVWXZYabcdefghigklmnopqrstuvwxyz0123456789\r\n"
#define BUFFERSIZE 1024
#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)


struct usr_context{
	char serverip[16];
	int port;
	int threadnum;
	int connection;
	int requestion;
    int failed;
};

int init_client(const char*ip ,unsigned short port)//socket connect
{
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0)
    {
        printf("socket failed\n");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(struct sockaddr_in));
    addr.sin_addr.s_addr=inet_addr(ip);
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    socklen_t socklen=sizeof(struct sockaddr_in);
    if(connect(sockfd,(struct sockaddr*)&addr,socklen)<0)
    {
        printf("connect failed\n");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int  send_and_recv(struct usr_context* ctx,int fd)
{
    char recvbuf[BUFFERSIZE]={0};
    if(send(fd,MESAGE,strlen(MESAGE),0)<0)
    {
     
        printf("send failed\n");
        return -1;
    }
    if(recv(fd,recvbuf,BUFFERSIZE,0)<=0)
    {
     
        printf("recv failed\n");
        return -1;
    }
    if(strcmp(MESAGE,recvbuf)!=0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

void* working(void* arg)
{
    struct usr_context *ctx=(struct usr_context*) arg;
    int count=ctx->requestion/ctx->threadnum  ;
    int connfd=init_client(ctx->serverip,ctx->port);
    if(connfd<0)
    {
        __sync_fetch_and_add(&ctx->failed, 1);
        return NULL;
    }
    for(int i=0;i<count;++i)
    {
        int ret=send_and_recv(ctx,connfd);
        if(ret<0)
        {
            __sync_fetch_and_add(&ctx->failed, count);
        }
    }
    close(connfd);
    return NULL;
}

// s p t c n    serverip  port threadnum   connections  requstions
int main(int argc,char** argv)
{
    int opt;
    struct usr_context ctx;
    memset(&ctx,0,sizeof(struct usr_context));
    while((opt=getopt(argc,argv,"s:p:t:c:n:?"))!=-1)
    {
        switch(opt)
        {
            case 's':
            strcpy(ctx.serverip,optarg);
            break;
            case 'p':
            ctx.port=atoi(optarg);
            break;
            case 't':
            ctx.threadnum=atoi(optarg);
            break;
            case 'c':
            ctx.connection=atoi(optarg);
            break;
            case 'n':
            ctx.requestion=atoi(optarg);
            break;
            default:
            printf("failed to parse command\n");
        }

    }
    pthread_t *pthreadnums=(pthread_t*)malloc(sizeof(pthread_t)*ctx.threadnum);
    struct timeval begin;
    gettimeofday(&begin,NULL);
    for(int i=0;i<ctx.threadnum;++i)
    {
        pthread_create(&pthreadnums[i],NULL,working,&ctx);
    }
    for(int i=0;i<ctx.threadnum;++i)
    {
        pthread_join(pthreadnums[i],NULL);
    }
    struct timeval end;
    gettimeofday(&end,NULL);

	int time_used = TIME_SUB_MS(end, begin);

	
	printf("success: %d, failed: %d, time_used: %d, qps: %d\n", ctx.requestion-ctx.failed, 
		ctx.failed, time_used, ctx.requestion * 1000 / time_used);


    return 0;
}