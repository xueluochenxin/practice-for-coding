#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <netdb.h>
#include <fcntl.h>


#define HTTP_VERSION		"HTTP/1.1"
#define CONNETION_TYPE		"Connection: close\r\n"

#define BUFFER_SIZE		4096

char *host_to_ip(const char *hostname) {

	struct hostent *host_entry = gethostbyname(hostname); //dns

	// 14.215.177.39 --> 
	//inet_ntoa ( unsigned int --> char *
	// 0x12121212 --> "18.18.18.18"
	if (host_entry) {
		return inet_ntoa(*(struct in_addr*)*host_entry->h_addr_list);
	} 

	return NULL;
}
int http_create_socket(char *ip) {

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in sin = {0};
	sin.sin_family = AF_INET;
	sin.sin_port = htons(80); //
 	sin.sin_addr.s_addr = inet_addr(ip);

	if (0 != connect(sockfd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in))) {
		return -1;
	}

	fcntl(sockfd, F_SETFL, O_NONBLOCK);//设置非阻塞

	return sockfd;

}
char* http_send_request(char* hostname,char *resource)
{
char* ip=host_to_ip(hostname);
int sockfd=http_create_socket(ip);

char buffer[BUFFER_SIZE]={0};
	sprintf(buffer, 
"GET %s %s\r\n\
Host: %s\r\n\
%s\r\n\
\r\n",

	resource, HTTP_VERSION,
	hostname,
	CONNETION_TYPE
	);
send(sockfd, buffer, strlen(buffer), 0);
fd_set fdread;
	
	FD_ZERO(&fdread);
	FD_SET(sockfd, &fdread);

	
	char *result = malloc(sizeof(int));
	memset(result, 0, sizeof(int));

    while (1) {
        // 修复：tv 必须放在循环内部，因为 select 会修改它的值
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        // 门卫探测有没有水（数据可读）
        int selection = select(sockfd + 1, &fdread, NULL, NULL, &tv);
        if (selection <= 0 || !FD_ISSET(sockfd, &fdread)) {
            // 超时(0) 或者 出错(-1) 或者 不是当前socket可读
            break;
        }

        // 接水
        memset(buffer, 0, BUFFER_SIZE);
        int len = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        
        if (len == 0) { 
            // 对方发完数据，关闭了 TCP 连接（水管关了）
            break; 
        }
        
        if (len < 0) { 
            // 非阻塞模式下，可能出错或暂时没数据（EAGAIN）
            break; 
        }

        // 拼接水（扩展水池，然后把新接的水倒进去）
        // 注意 result 的大小 = 原来大小 + 新接大小 + 1个'\0'位置
        char *temp = realloc(result, strlen(result) + len + 1);
        if (temp == NULL) {
            // 内存分配失败，跳出循环防止崩溃
            break;
        }
        result = temp;
        
        // 用 strncat 或 memcpy 拼接到末尾
        // strncat(result, buffer, len); // 也可以，但 memcpy 更快
        memcpy(result + strlen(result), buffer, len);
    }
      close(sockfd); // 必须关闭套接字
    return result;
}







int main(int argc,char ** argv)// 程序  网址  资源路径
{
if (argc < 3) {
        printf("Usage: %s <hostname> <resource>\n", argv[0]);
        return -1;
    }

    char *response = http_send_request(argv[1], argv[2]);
    if (response != NULL) {
        printf("========== 服务器响应 ==========\n");
        printf("%s\n", response);
        printf("================================\n");
        free(response); // 释放内存
    }

    return 0;
}