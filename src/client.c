#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

int main(int argc, char *argv[])
{
    if(argc < 3)
    {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        exit(1);
    }
    const char *server_ip = argv[1];
    int port = atoi(argv[2]);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
    {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(server_ip),
        .sin_port = htons(port)
    };

    if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        close(fd);
        perror("connect");
        exit(1);
    }

    printf("client connect server.\n");

    char buf[PROTOCOL_MAX_BODY_SIZE + 1];
    while(fgets(buf, sizeof(buf), stdin))
    {
        // 去掉末尾换行符
        int len = strlen(buf);
        if(len > 0 && buf[len - 1] == '\n')
        {
            buf[len - 1] = '\0';
            len--;
        }

        // 判断是否退出
        if(strcmp(buf, "quit") == 0)
        {
            protocol_send_msg(fd, MSG_QUIT, "quit");
            break;
        }

        // 发送消息
        if(protocol_send_msg(fd, MSG_TEXT, buf) < 0)
        {
            perror("send");
            break;
        }

        // 接收回显
        uint8_t type;
        char msg[PROTOCOL_MAX_BODY_SIZE + 1];
        int ret = protocol_recv_msg(fd, &type, msg, sizeof(msg));
        if(ret <= 0)
        {
            if(ret == 0) printf("server close.\n");
            else perror("recv");
            break;
        }

        printf("Echo: %s\n", msg);
    }

    close(fd);
    printf("client exit.\n");
    return 0;
}
