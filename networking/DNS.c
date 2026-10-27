//192.168.74.128

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <time.h>
#include <arpa/inet.h>

#define DNS_SERVER_PORT		53
#define DNS_SERVER_IP		"114.114.114.114"

struct dns_header{
unsigned short transaction_id;//会话标识
unsigned short flags;//标志
unsigned short numbers;//问题数
unsigned short answer_rr;
unsigned short athority_rr;
unsigned short additional_rr;
};
struct dns_question{
int lenth;
char *name;
unsigned short type;
unsigned short class;
};
struct dns_item{
    char* domain;
    char* ip;
};
int dns_header_create(struct dns_header* header)
{
    if(header==NULL) return -1;
    memset(header,0,sizeof(struct dns_header));

    srand(time(NULL));
    header->transaction_id=htons(rand());

    header->flags=htons(0x0100);
    header->numbers=htons(1);
    return 0;
}
//3www60voice3com
int dns_query_create(struct dns_question *question,const char* hostname)
{
    if(question==NULL||hostname==NULL)
    {
        return -1;
    }
    question->lenth=strlen(hostname)+2;
    question->name=(char*)malloc(strlen(hostname)+2);
    if(question->name==NULL){return -2;}

    question->type=htons(1);
    question->class=htons(1);
    
    char* tempname=(char*)malloc(question->lenth);
    if(tempname==NULL) return -2;
    // 修正：避免多拷贝越界字节。只拷贝字符串本身的长度+1（包含\0）
    memcpy(tempname,hostname,strlen(hostname)+1); 

    char* delim=".";
    
    char* token=strtok(tempname,delim);
    char* numptr=question->name;
    while(token!=NULL)
    {
        int len=strlen(token);
        *numptr=len;
       numptr++;

       strcpy(numptr,token);
       numptr+=len;

       token=strtok(NULL,delim);
    }


    free(tempname);
    return 0;
}

int dns_build_request(struct dns_header* header,struct dns_question *question,char* request,int rlen)
{
 if (header == NULL || question == NULL || request == NULL) return -1;
    memset(request, 0, rlen); // 1. 清理场地

    // 2. 放第一个包裹：Header
    memcpy(request, header, sizeof(struct dns_header));
    int offset = sizeof(struct dns_header); // 记录传送带当前的位置（偏移量）

    // 3. 放第二个包裹：转换后的域名（3www60voice3com0）
    memcpy(request+offset, question->name, question->lenth);
    offset += question->lenth;

    // 4. 放第三个包裹：Type（查询类型）
    memcpy(request+offset, &question->type, sizeof(question->type));
    offset += sizeof(question->type);

    // 5. 放第四个包裹：Class（查询类）
    memcpy(request+offset, &question->class, sizeof(question->class));
    offset += sizeof(question->class);

    return offset; // 6. 返回整个包裹的总长度
}

// ================== 新增的 DNS 解析部分 ==================

// 判断是否是 DNS 压缩指针（前两位为 11，即 0xC0）
static int is_pointer(int in) {
    return ((in & 0xC0) == 0xC0);
}

// 解析 DNS 名字（处理压缩指针跳转）
static void dns_parse_name(unsigned char *chunk, unsigned char *ptr, char *out, int *len) {
    int flag = 0;
    char *pos = out + (*len);

    while (1) {
        flag = (int)ptr[0];
        if (flag == 0) break;

        if (is_pointer(flag)) {
            // 压缩指针：偏移量为低14位
            int offset = ((flag & 0x3F) << 8) | ptr[1];
            dns_parse_name(chunk, chunk + offset, out, len);
            break; // 递归跳转解析完，直接跳出当前循环
        } else {
            // 普通标签：长度 + 内容
            ptr++;
            memcpy(pos, ptr, flag);
            pos += flag;
            ptr += flag;

            *len += flag;
            // 如果不是结尾的0，补上点号
            if ((int)ptr[0] != 0) {
                memcpy(pos, ".", 1);
                pos += 1;
                (*len) += 1;
            }
        }
    }
}

int dns_parse_response(char* response,struct dns_item** dns_domain)
{
    unsigned char *ptr = (unsigned char *)response;
    
    // 1. 跳过 Header 前4字节（ID 2字节 + Flags 2字节）
    ptr += 4;
    int querys = ntohs(*(unsigned short *)ptr); ptr += 2; // 问题数
    int answers = ntohs(*(unsigned short *)ptr); ptr += 2; // 回答数
    ptr += 4; // 跳过 Authority(2) + Additional(2)

    // 2. 跳过 Question 区
    for (int i = 0; i < querys; i++) {
        while (1) {
            int flag = (int)ptr[0];
            ptr += (flag + 1);
            if (flag == 0) break;
        }
        ptr += 4; // 跳过 Type(2) + Class(2)
    }

    // 3. 准备结果数组
    struct dns_item *list = (struct dns_item *)calloc(answers, sizeof(struct dns_item));
    if (list == NULL) {
        return -1;
    }

    char aname[256] = {0};
    char ip[16] = {0};
    int cnt = 0;

    // 4. 遍历 Answer 区
    for (int i = 0; i < answers; i++) {
        memset(aname, 0, sizeof(aname));
        int len = 0;

        // 解析域名
        dns_parse_name((unsigned char *)response, ptr, aname, &len);

        // 关键：移动 ptr 跳过刚刚解析的名字
        // 如果是指针（0xC0开头），占2字节；如果是普通名字，按长度跳过
        if (is_pointer(ptr[0])) {
            ptr += 2;
        } else {
            while (*ptr != 0) {
                ptr += (*ptr) + 1;
            }
            ptr += 1; // 跳过最后的0
        }

        int type = ntohs(*(unsigned short *)ptr); ptr += 2; // 读取 Type
        ptr += 2; // 跳过 Class
        int ttl = ntohl(*(unsigned int *)ptr); ptr += 4;    // 读取 TTL（4字节，用ntohl）
        int datalen = ntohs(*(unsigned short *)ptr); ptr += 2; // 读取数据长度

        if (type == 1) { // A 记录
            if (datalen == 4) {
                inet_ntop(AF_INET, ptr, ip, sizeof(ip));
                printf("%s has address %s\n", aname, ip);
                printf("\tTime to live: %d minutes , %d seconds\n", ttl / 60, ttl % 60);

                list[cnt].domain = (char *)calloc(strlen(aname) + 1, 1);
                strcpy(list[cnt].domain, aname);

                list[cnt].ip = (char *)calloc(strlen(ip) + 1, 1);
                strcpy(list[cnt].ip, ip);

                cnt++;
            }
        }
        ptr += datalen; // 移动到下一个 Answer
    }

    *dns_domain = list;
    return cnt;
}

// =========================================================

int dns_client_commit(const char* domain)
{
    int sockfd=socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd<0)
    {
        fprintf(stderr,"socket wrong");
        return -1;
    }
    
    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(DNS_SERVER_PORT);
	servaddr.sin_addr.s_addr = inet_addr(DNS_SERVER_IP);

    //准备报文
    struct dns_header header={0};
    struct dns_question query={0};
    dns_header_create(&header);
    dns_query_create(&query,domain);

    char request[1024];
    int length=dns_build_request(&header,&query,request,1024);

    int slen=sendto(sockfd,request,length,0,(struct sockaddr*)&servaddr,sizeof(struct sockaddr_in));
    
    char response[1024]={0};
    struct sockaddr_in addr;
	size_t addr_len = sizeof(struct sockaddr_in);
    int n = recvfrom(sockfd, response, sizeof(response), 0, (struct sockaddr*)&addr, (socklen_t*)&addr_len);

    struct dns_item *dns_domain = NULL;
	int count=dns_parse_response(response, &dns_domain);
    for (int i = 0; i < count; i++) {   //count是dns parse解析出来的ip数量
    free(dns_domain[i].domain);
    free(dns_domain[i].ip);
    }
free(dns_domain); // 最后释放数组本身
close(sockfd);
return count;
}

int main(int argc,char **argv)
{
     if (argc < 2) {
        printf("Usage: %s <domain>\n", argv[0]);
        return -1;
    }
    dns_client_commit(argv[1]); // 传入域名，如 www.baidu.com
    
    return 0;
}