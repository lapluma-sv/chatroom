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
    uint8_t type;
    char msg[PROTOCOL_MAX_BODY_SIZE + 1];
    char buf[PROTOCOL_MAX_BODY_SIZE + 1];

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
    printf("请输入昵称: ");
    fgets(buf, sizeof(buf), stdin);
    if(buf[strlen(buf) - 1] == '\n') 
    {
        buf[strlen(buf) - 1] = '\0';
    }
    protocol_send_msg(fd, MSG_NICKNAME, buf);
    if(protocol_recv_msg(fd, &type, msg, sizeof(msg)) > 0 && type == MSG_SYSTEM) 
    {
        printf("%s\n", msg);
    }

    fd_set readfds;
    
    while(1)
    {
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);       // 0 = stdin
        FD_SET(fd, &readfds);

        if(select(fd + 1, &readfds, NULL, NULL, NULL) < 0)
        {
            perror("select");
            break;
        }

        if(FD_ISSET(0, &readfds))
        {
            fgets(buf, sizeof(buf), stdin);
            int len = strlen(buf);
            if(len > 0 && buf[len - 1] == '\n')
            {
                buf[len - 1] = '\0';
                len--;
            }
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
        }
        else if(FD_ISSET(fd, &readfds))
        {
            int ret = protocol_recv_msg(fd, &type, msg, sizeof(msg));
            if(ret == 0) 
            {
                printf("server close.\n");
                break;
            }
            else if(ret < 0)
            {
                perror("recv");
                break;
            }
            printf("%s\n", msg);
            fflush(stdout);
        }
    }

    close(fd);
    printf("client exit.\n");
    return 0;
}
