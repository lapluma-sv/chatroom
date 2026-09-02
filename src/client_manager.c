#include "client_manager.h"



static client_info_t *clients[MAX_CLIENTS];
static int client_count = 0;
static int epoll_fd = 0;
static struct epoll_event ev;



int client_manager_init(int fd)
{
    epoll_fd = fd;
    memset(clients, 0, sizeof(clients));
    client_count = 0;
    return 0;
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



void client_remove(int fd)
{
    if(clients[fd] == NULL)
    {
        return;
    }
    char msg[PROTOCOL_MAX_BODY_SIZE] = "";
    if(clients[fd]->has_nickname)
    {
        snprintf(msg, sizeof(msg), "%s 离开了聊天室", clients[fd]->nickname);
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    free(clients[fd]);
    clients[fd] = NULL;
    client_count--;
    printf("客户端退出，fd = %d，当前客户端数 = %d\n", fd, client_count);
    broadcast_system(msg);
}
client_info_t *client_get(int fd)
{
    return clients[fd];
}
int client_get_count()
{
    return client_count;
}



void client_set_nickname(int fd, const char *nick)
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



void client_check_alive(void)
{
    int dead_fds[MAX_CLIENTS];
    int dead_cnt = 0;

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        client_info_t *c = clients[i];
        if(c == NULL) 
        {
            continue;
        }

        char probe;
        int ret = recv(c->fd, &probe, 1, MSG_PEEK | MSG_DONTWAIT);

        if(ret == 0)
        {
            // 对端已关闭（EOF），fd 还挂在表里 → 僵尸
            dead_fds[dead_cnt++] = c->fd;
        }
        else if(ret < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 缓冲区空 = 连接健康空闲，跳过
            }
            else
            {
                // EPIPE / EBADF / ECONNRESET 等 → 连接已坏死
                printf("[诊断] 巡检失败 fd=%d ret=%d errno=%d(%s)\n",
                       c->fd, ret, errno, strerror(errno));
                dead_fds[dead_cnt++] = c->fd;
            }
        }
        // ret > 0：有数据在排队，正常，主循环会处理

    }

    for(int k = 0; k < dead_cnt; k++)
    {
        client_remove(dead_fds[k]);
    }
}