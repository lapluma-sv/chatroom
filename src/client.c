#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

static void drain_socket(int fd)
{
    uint8_t type;
    char msg[PROTOCOL_MAX_BODY_SIZE + 1];

    while(1)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };  // 每轮重置！

        int sel = select(fd + 1, &readfds, NULL, NULL, &tv);
        if(sel <= 0) break;          // 超时没数据 / 出错 → 排空结束

        int ret = protocol_recv_msg(fd, &type, msg, sizeof(msg));
        if(ret <= 0) break;          // 读完、对端关、EAGAIN、出错 → 结束
        printf("%s\n", msg);         // 还有数据就继续下一轮
    }
}

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
    char buff[64] = {0};
    while(1)
    {
        printf("请输入昵称: ");
        if(fgets(buff, sizeof(buff), stdin) == NULL)
        {
            // EOF/输入错误：流已关闭，没有下一次输入，不能重试（防死循环）
            printf("\n[诊断] stdin 已关闭，退出\n");
            close(fd);
            exit(1);
        }
        size_t len = strlen(buff);
        if(len > 0 && buff[len - 1] == '\n')
        {
            // 找到换行 = 整行读入，stdin 干净，去换行即可
            buff[len - 1] = '\0';
            len--;
        }
        else
        {
            // 没找到换行 = 行超长被截断，残留还在 stdin，排空到行尾
            int c;
            while((c = getchar()) != '\n' && c != EOF) { }
            if(c == EOF)
            {
                printf("\n[诊断] stdin 已关闭，退出\n");
                close(fd);
                exit(1);
            }
            printf("昵称长度不能超过%d个字符, 请重新输入\n", (int)sizeof(buff) - 2);
            continue;
        }
        if(len == 0)
        {
            printf("昵称不能为空, 请重新输入\n");
            continue;
        }
        break;
    }
    char mynick[64];
    snprintf(mynick, sizeof(mynick), "%s", buff);   // 记住自己的身份，便于日志对照
    int sent_count = 0;
    protocol_send_msg(fd, MSG_NICKNAME, buff);
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
            if(fgets(buf, sizeof(buf), stdin) == NULL)
            {
                if(feof(stdin))
                    printf("[诊断] [%s] 已发送%d条后 stdin EOF\n", mynick, sent_count);
                else
                    printf("[诊断] [%s] stdin 错误: %s\n", mynick, strerror(errno));
                break;
            }
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
                printf("[诊断] [%s] 第%d条发送失败: %s\n", mynick, sent_count + 1, strerror(errno));
                break;
            }
            sent_count++;
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

    drain_socket(fd);

    close(fd);
    printf("client exit.\n");
    return 0;
}
