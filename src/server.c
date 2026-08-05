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
#define MAX_CLIENTS 1024
#define MAX_NICK_LEN 32

typedef struct {
    int fd;
    char nickname[MAX_NICK_LEN];    
    int has_nickname;               
} client_info_t;

struct epoll_event events[MAX_EVENTS];
volatile sig_atomic_t running = 1;
client_info_t *clients[MAX_CLIENTS];
int client_count = 0;
int epoll_fd = 0; 
struct epoll_event ev;

void sigint_handler(int signum)
{
    (void)signum;
    running = 0;
}

void client_join(int fd)
{
    if (fd >= MAX_CLIENTS) 
    {
        close(fd);
        return;
    }
    client_info_t *client = malloc(sizeof(client_info_t));
    if(client == NULL)
    {
        perror("malloc");
        exit(1);
    }
    client->fd = fd;
    memset(client->nickname, 0, sizeof(client->nickname));
    client->has_nickname = 0;
    clients[fd] = client;

    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        perror("epoll_ctl");
        free(client);
        clients[fd] = NULL;
        close(fd);
        return;
    }
    client_count++;
    printf("新连接加入，fd = %d，当前客户端数 = %d\n", fd, client_count);
}

void broadcast_system(const char *msg)
{
    char buf[PROTOCOL_MAX_BODY_SIZE];
    snprintf(buf, sizeof(buf), "[系统] %s", msg);
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clients[i] != NULL && clients[i]->has_nickname)
        {
            protocol_send_msg(clients[i]->fd, MSG_SYSTEM, buf);
        }
    }
}
void client_remove(int fd)
{
    if(clients[fd] == NULL)
    {
        return;
    }
    if(clients[fd]->has_nickname)
    {
        char msg[PROTOCOL_MAX_BODY_SIZE];
        snprintf(msg, sizeof(msg), "%s 离开了聊天室", clients[fd]->nickname);
        broadcast_system(msg);
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    free(clients[fd]);
    clients[fd] = NULL;
    client_count--;
    printf("客户端退出，fd = %d，当前客户端数 = %d\n", fd, client_count);
}


void broadcast_text(int sender_fd, const char *msg)
{
    char buf[PROTOCOL_MAX_BODY_SIZE];
    snprintf(buf, sizeof(buf), "[text]%s : %s", clients[sender_fd]->nickname, msg);
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clients[i] != NULL && clients[i]->has_nickname && i != sender_fd)
        {
            protocol_send_msg(clients[i]->fd, MSG_TEXT, buf);
        }
    }
}

void handle_nickname(int fd, const char *nick)
{
    if(clients[fd]->has_nickname)
    {
        printf("昵称已存在\n");
        return;
    }
    if(strlen(nick) >= MAX_NICK_LEN)
    {
        printf("昵称长度超过最大限制\n");
        return;
    }
    strncpy(clients[fd]->nickname, nick, MAX_NICK_LEN - 1);
    clients[fd]->nickname[MAX_NICK_LEN - 1] = '\0';
    clients[fd]->has_nickname = 1;
    printf("fd = %d 昵称已设置，昵称 = %s\n", fd, nick);
    char msg[PROTOCOL_MAX_BODY_SIZE];
    snprintf(msg, sizeof(msg), "%s 加入了聊天室", nick);
    broadcast_system(msg);
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
    epoll_fd = epoll_create1(0);
    if(epoll_fd < 0)
    {
        perror("epoll_create1");
        exit(1);
    }
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
                    handle_nickname(events[i].data.fd, msg);
                }
            }
        }
    }
    close(server_fd);
    close(epoll_fd);
    printf("server close.\n");
    return 0;
}