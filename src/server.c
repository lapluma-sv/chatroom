#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/epoll.h>
#include "protocol.h"
#include "broadcast.h"
#include "client_manager.h"

#define MAX_EVENTS 64

volatile sig_atomic_t running = 1;

void sigint_handler(int signum)
{
    (void)signum;
    running = 0;
}

int main(void)
{
    int epoll_fd = 0; 
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];
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
    epoll_fd = epoll_create1(0);
    if(epoll_fd < 0)
    {
        perror("epoll_create1");
        exit(1);
    }

    client_manager_init(epoll_fd);

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
                client_join(client_fd);
            }
            else
            {
                uint8_t type;
                char msg[PROTOCOL_MAX_BODY_SIZE + 1];
                if(protocol_recv_msg(events[i].data.fd, &type, msg, sizeof(msg)) <= 0)
                {
                    client_remove(events[i].data.fd);
                }
                if(type == MSG_TEXT)
                {
                    printf("recv [fd=%d]: type = %s, msg = %s\n", events[i].data.fd, protocol_type_str(type), msg);
                    broadcast_text(events[i].data.fd, msg);
                }
                else if(type == MSG_QUIT)
                {
                    client_remove(events[i].data.fd);
                }
                else if(type == MSG_NICKNAME)
                {
                    client_set_nickname(events[i].data.fd, msg);
                }
            }
        }
    }
    close(server_fd);
    close(epoll_fd);
    printf("server close.\n");
    return 0;
}