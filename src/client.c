#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(void)
{
    // 1. 创建 socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) 
    {
        perror("socket");
        exit(1);
    }

    // 2. 连接服务端
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // 连接本机
    addr.sin_port = htons(8888);
    if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) 
    {
        perror("connect");
        exit(1);
    }

    // 3. 循环读取输入并发送
    char buf[1024];
    while (fgets(buf, sizeof(buf), stdin)) 
    {
        if(send(fd, buf, strlen(buf), 0) == -1) 
        {
            perror("send");
            exit(1);
        }
        printf("client send : %s", buf);
        if(strcmp(buf, "quit\n") == 0) 
        {
            break;
        }
        int len = recv(fd, buf, sizeof(buf), 0);
        if(len <= 0) 
        {
            break;
        }
        buf[len] = '\0';
        printf("Echo: %s", buf);
    }

    // 4. 关闭连接
    close(fd);
}
