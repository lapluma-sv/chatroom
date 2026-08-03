#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include "protocol.h"

volatile sig_atomic_t running = 1;

void sigint_handler(int signum)
{
    (void)signum;
    running = 0;
}

int main(void)
{
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(8888)
    };

    if(bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        close(server_fd);
        perror("bind");
        exit(1);
    }

    if(listen(server_fd, 5) == -1)
    {
        close(server_fd);
        perror("listen");
        exit(1);
    }

    printf("server start.\n");

    while(running)
    {
        int client_fd = accept(server_fd, NULL, NULL);
        if(client_fd < 0)
        {
            if(errno == EINTR) break;
            perror("accept");
            break;
        }
        printf("server accept client.\n");

        while(1)
        {
            uint8_t type;
            char msg[PROTOCOL_MAX_BODY_SIZE + 1];

            // 读取一条完整消息（内部处理了粘包/拆包）
            int ret = protocol_recv_msg(client_fd, &type, msg, sizeof(msg));
            if(ret <= 0)
            {
                if(ret == 0) printf("client quit.\n");
                else perror("protocol_recv_msg");
                break;
            }

            // 收到退出消息，直接断开
            if(type == MSG_QUIT)
            {
                break;
            }

            printf("recv: type = %s, msg = %s\n", protocol_type_str(type), msg);

            // Echo：原样发回去
            if(protocol_send_msg(client_fd, type, msg) < 0)
            {
                perror("protocol_send_msg");
                break;
            }
        }

        close(client_fd);
        printf("client exit.\n");
    }

    close(server_fd);
    printf("server close.\n");
    return 0;
}
