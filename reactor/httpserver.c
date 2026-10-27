#define _GNU_SOURCE       //  让 off_t 是 64 位
#include <stdio.h>        // printf, perror
#include <stdlib.h>       // exit, malloc
#include <string.h>       // memset, bzero
#include <unistd.h>       // close, read, write
#include <errno.h>        // errno
#include <fcntl.h>        // fcntl (如果要用非阻塞 ET 模式)
#include <sys/socket.h>   // socket, bind, listen, accept, recv, send
#include <netinet/in.h>   // sockaddr_in, htons, htonl, INADDR_ANY
#include <arpa/inet.h>    // inet_ntop, inet_addr (打印客户端 IP)
#include <sys/epoll.h>    // epoll_create1, epoll_ctl, epoll_wait, struct epoll_event
#include <sys/sendfile.h> //  sendfile
#include <sys/stat.h>     //  fstat, struct stat
#include <signal.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

#define BUFFER_SIZE 1024
#define CONN_SIZE 65536
#define ADD_FLAG 1
#define MOD_FLAG 0
#define MAX_EVENTS 1024
#define STATUS_HEAD 0
#define STATUS_BODY 1
#define STATUS_END 2

typedef int (*CALLBACK)(int fd);

int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);
int set_nonblocking(int fd);
int set_event(int fd, int event, int flag);
int is_websocket_request(const char *buf);

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
    int status;      // 0=发头, 1=发body, 2=收尾
    int filefd;      // 要发送的文件 fd
    off_t file_size; // 文件总大小
    off_t file_sent; // 已发送字节数

    int is_websocket;//  0代表HTTP 1代表websocket
};

//quanjubianliang
int epfd = -1;
struct conn conn_list[CONN_SIZE] = {0};
//quanjubianliang

int is_websocket_request(const char *buf) {
    // 简单查找关键头部
    return (strstr(buf, "Upgrade: websocket") != NULL ||
            strstr(buf, "Upgrade: WebSocket") != NULL) &&
           (strstr(buf, "Sec-WebSocket-Key:") != NULL);
}

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

int ws_send_frame(int fd, int opcode, const char *data, int len)
{
    struct conn *c = &conn_list[fd];
    unsigned char frame[BUFFER_SIZE];
    int pos = 0;

    frame[pos++] = 0x80 | (opcode & 0x0F);   // FIN=1 + opcode

    if (len <= 125) {
        frame[pos++] = len & 0x7F;
    } else if (len <= 65535) {
        frame[pos++] = 126;
        frame[pos++] = (len >> 8) & 0xFF;
        frame[pos++] = len & 0xFF;
    } else {
        return -1;   // 简化：不支持超大帧
    }

    if (pos + len > BUFFER_SIZE) return -1;
    memcpy(frame + pos, data, len);
    pos += len;

    // 把拼好的帧放进 wbuffer
    memcpy(c->wbuffer, frame, pos);
    c->wlen = pos;

    // 同步发送，和 HTTP 分支保持一致
    send_cb(fd);
    return 0;
}

int web_socket_parse(int fd)
{
    struct conn *c = &conn_list[fd];
    unsigned char *buf = (unsigned char *)c->rbuffer;
    int len = c->rlen;

    if (len < 2) return 0;

    int opcode = buf[0] & 0x0F;
    int masked = (buf[1] & 0x80) ? 1 : 0;
    unsigned long long payload_len = buf[1] & 0x7F;
    int pos = 2;

    if (payload_len == 126) {
        if (len < 4) return 0;
        payload_len = (buf[2] << 8) | buf[3];
        pos = 4;
    } else if (payload_len == 127) {
        if (len < 10) return 0;
        payload_len = 0;
        for (int i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | buf[pos + i];
        pos = 10;
    }

    unsigned char mask[4] = {0};
    if (masked) {
        if (len < pos + 4) return 0;
        memcpy(mask, buf + pos, 4);
        pos += 4;
    }

    if ((unsigned long long)len < pos + payload_len) {
        printf("ws frame incomplete, drop\n");
        return 0;
    }

    char *payload = malloc(payload_len + 1);
    if (!payload) return -1;
    for (unsigned long long i = 0; i < payload_len; i++) {
        payload[i] = masked ? (buf[pos + i] ^ mask[i % 4]) : buf[pos + i];
    }
    payload[payload_len] = '\0';

    switch (opcode) {
        case 0x1:   // 文本
            printf("ws text: %s\n", payload);
            ws_send_frame(fd, 0x1, payload, payload_len);
            break;

        case 0x2:   // 二进制
            printf("ws binary: %llu bytes\n", payload_len);
            ws_send_frame(fd, 0x2, payload, payload_len);
            break;

        case 0x8:   // 关闭
            printf("ws close\n");
            ws_send_frame(fd, 0x8, "", 0);   // 回 close 帧
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            break;

        case 0x9:   // Ping → Pong
            ws_send_frame(fd, 0xA, payload, payload_len);
            break;

        case 0xA:   // Pong，忽略
            break;

        default:
            printf("ws unknown opcode %d\n", opcode);
            break;
    }

    free(payload);
    return 0;
}

int wb_socket_set(int fd)
{
    char key[256]={0};
    char *ptr=strstr(conn_list[fd].rbuffer,"Sec-WebSocket-Key:");
    if(ptr==NULL)
    {
        return -1;
    }
    ptr+=strlen("Sec-WebSocket-Key:");
    while(*ptr==' ')
    {
        ptr++;
    }
    int i=0;
    while(*ptr!='\r'&&*ptr!='\n'&&*ptr!='\0')
    {
        key[i]=*ptr;
        i++;ptr++;
    }
    char accept_src[512]={0};
    snprintf(accept_src, sizeof(accept_src), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    //先哈希再base64编码处理
        unsigned char sha1[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)accept_src, strlen(accept_src), sha1);

    char accept_b64[128];
    EVP_EncodeBlock((unsigned char*)accept_b64, sha1, SHA_DIGEST_LENGTH);

    conn_list[fd].wlen = sprintf(conn_list[fd].wbuffer,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept_b64);
    
    conn_list[fd].is_websocket=1;
    send_cb(fd);
    
    return 0;
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
        //
        conn_list[clientfd].file_sent = 0;
        conn_list[clientfd].file_size = 0;
        conn_list[clientfd].filefd = -1;
        conn_list[clientfd].status = 0;
        conn_list[clientfd].is_websocket=0;
        set_event(clientfd, EPOLLIN, ADD_FLAG);
    }
    return 0;
}

int recv_cb(int fd)
{
    int count = recv(fd, conn_list[fd].rbuffer+conn_list[fd].rlen, BUFFER_SIZE - 1-conn_list[fd].rlen, 0);
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
    // if处理完了，下面的count>0也就是进行了正常读取
    //  recv不会自动加入\0  需要我们手动加进去
    conn_list[fd].rbuffer[count] = '\0';
    conn_list[fd].rlen = count;

    //
    if(conn_list[fd].is_websocket)
    {
        web_socket_parse(fd);
    }
    else
    {
        if(is_websocket_request(conn_list[fd].rbuffer))
        {
           wb_socket_set(fd);  
        }
        else
        {
            conn_list[fd].filefd = open("dsh.png", O_RDONLY);
            struct stat st;
            if (fstat(conn_list[fd].filefd, &st) < 0)
            {
                perror("fstat");
                close(conn_list[fd].filefd);
                return -1;
            }
            conn_list[fd].file_sent = 0;
            conn_list[fd].file_size = st.st_size;

            const char *ctype = "image/png";
            conn_list[fd].wlen = sprintf(conn_list[fd].wbuffer,
                                         "HTTP/1.1 200 OK\r\n"
                                         "Content-Type: %s\r\n"
                                         "Content-Length: %ld\r\n"
                                         "Connection: close\r\n"
                                         "\r\n",
                                         ctype,
                                         (long)st.st_size);

            conn_list[fd].status = STATUS_HEAD;
            send_cb(fd);
        }
    }






    return count;
}

int send_cb(int fd)
{
    if(!conn_list[fd].is_websocket)
    {
    struct conn *c = &conn_list[fd];
    if (c->status == STATUS_HEAD)
    {
        int n = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlen, 0);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* 头还没发完，等下次 EPOLLOUT */
                set_event(fd, EPOLLIN | EPOLLOUT, MOD_FLAG);
                return 0;
            }
            if (errno == EINTR)
                return 0;
            perror("send header");
            return -1;
        }
        else // n>0
        {
            c->wlen -= n;
            if (c->wlen > 0)
            {
                /* 只发了一部分，挪一下继续等 */
                memmove(c->wbuffer, c->wbuffer + n, c->wlen);
                set_event(fd, EPOLLIN | EPOLLOUT, MOD_FLAG);
                return 0;
            }
        }
        c->status = STATUS_BODY;
    }
    if (c->status == STATUS_BODY)
    {
        if (c->filefd < 0 || c->file_sent >= c->file_size)
        {
            c->status = STATUS_END;
            return 0;
        }
        else
        {
            off_t offset = c->file_sent;
            size_t remain = c->file_size - c->file_sent;
            ssize_t n = sendfile(c->fd, c->filefd, &offset, remain);
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    set_event(fd, EPOLLIN | EPOLLOUT, MOD_FLAG);
                    return 0;
                }
                if (errno == EINTR)
                    return 0;
            }
            else
            {
                c->file_sent += n;
                if (c->file_sent < c->file_size)
                {
                    set_event(fd, EPOLLIN | EPOLLOUT, MOD_FLAG);
                    return n;
                }
            }
        }
        c->status = STATUS_END;
    }
    if (c->status == STATUS_END)
    {
        if (c->filefd >= 0)
        { 
            close(c->filefd);
            c->filefd = -1;
        }
        c->wlen = 0; 
        c->file_sent = 0;
        c->file_size = 0;
        c->status = 0;

        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        return 0;
    }
}
    else//websocket send
    {
        struct conn *c = &conn_list[fd];
        if (c->wlen <= 0)
        {
            // 没有待发数据，取消 EPOLLOUT，避免空转
            set_event(fd, EPOLLIN, MOD_FLAG);
            return 0;
        }

        int n = send(fd, c->wbuffer, c->wlen, 0);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                set_event(fd, EPOLLIN | EPOLLOUT, MOD_FLAG); 
                return 0; // 继续等 EPOLLOUT
            }
            if (errno == EINTR)
                return 0;
            perror("ws send");
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            return -1;
        }

        c->wlen -= n;
        if (c->wlen > 0)
        {
            // 还有剩，挪到前面，继续等 EPOLLOUT
            memmove(c->wbuffer, c->wbuffer + n, c->wlen);
            set_event(fd, EPOLLIN | EPOLLOUT, MOD_FLAG);
            return 0;
        }

        // 发完了：取消 EPOLLOUT，保留 EPOLLIN
        set_event(fd, EPOLLIN, MOD_FLAG);
        return 0;
    }
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
                if (Events[i].events & (EPOLLERR | EPOLLHUP)) // 出错  挂断
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, Events[i].data.fd, NULL);
                    close(Events[i].data.fd);
                    continue; // 处理完直接跳过，不要再走 IN/OUTS
                }
                if (Events[i].events & EPOLLIN)
                {
                    if (recv_cb(Events[i].data.fd) < 0)
                        continue; //  已关闭，跳过 OUT
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
    signal(SIGPIPE, SIG_IGN);
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