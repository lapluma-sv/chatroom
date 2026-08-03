#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/epoll.h>
#include "protocol.h"

#define MAX_EVENTS 64

struct epoll_event events[MAX_EVENTS];

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

    int client_count = 0;

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

    int epoll_fd = epoll_create1(0);
    if(epoll_fd < 0)
    {
        perror("epoll_create1");
        exit(1);
    }
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1)
    {
        perror("epoll_ctl");
        exit(1);
    }

    printf("server start.\n");

    while(running)
    {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if(n < 0)
        {
            perror("epoll_wait");
            break;
        }

        for(int i = 0; i < n; i++)
        {
            if(events[i].data.fd == server_fd)
            {
                int client_fd = accept(server_fd, NULL, NULL);
                if(client_fd < 0)
                {
                    perror("accept");
                    continue;
                }

                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
                {
                    perror("epoll_ctl");
                    close(client_fd);
                    continue;
                }
                client_count++;
                printf("新连接加入，fd = %d，当前客户端数 = %d\n", client_fd, client_count);
            }
            else
            {
                uint8_t type;
                char msg[PROTOCOL_MAX_BODY_SIZE + 1];
                if(protocol_recv_msg(events[i].data.fd, &type, msg, sizeof(msg)) <= 0)
                {
                    close(events[i].data.fd);
                    continue;
                }
                if(type == MSG_TEXT)
                {
                    printf("recv [fd=%d]: type = %s, msg = %s\n", events[i].data.fd, protocol_type_str(type), msg);
                    protocol_send_msg(events[i].data.fd, type, msg);
                }
                else if(type == MSG_QUIT)
                {
                    close(events[i].data.fd);
                    client_count--;
                    printf("客户端退出，fd = %d，当前客户端数 = %d\n", events[i].data.fd, client_count);
                }


            }
        }
    }
    close(server_fd);
    close(epoll_fd);
    printf("server close.\n");
    return 0;
}
