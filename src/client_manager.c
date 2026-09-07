#include "client_manager.h"



static client_info_t *clients[MAX_CLIENTS];
static int client_count = 0;
static int epoll_fd = 0;
static struct epoll_event ev;
static pthread_mutex_t g_clients_lock;



int client_manager_init(int fd)
{
    epoll_fd = fd;
    memset(clients, 0, sizeof(clients));
    client_count = 0;
    int ret = pthread_mutex_init(&g_clients_lock, NULL);
    if(ret != 0)
    {
        perror("pthread_mutex_init");
        exit(1);
    }
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
    pthread_mutex_lock(&g_clients_lock);
    clients[fd] = client;
    
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        perror("epoll_ctl");
        free(client);
        clients[fd] = NULL;
        close(fd);
        pthread_mutex_unlock(&g_clients_lock);
        return;
    }
    client_count++;
    int count = client_count;
    pthread_mutex_unlock(&g_clients_lock);
    printf("新连接加入，fd = %d，当前客户端数 = %d\n", fd, count);
}



void client_remove(int fd)
{
    pthread_mutex_lock(&g_clients_lock);
    if(clients[fd] == NULL)
    {
        pthread_mutex_unlock(&g_clients_lock);
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
    int count = client_count;
    pthread_mutex_unlock(&g_clients_lock);
    printf("客户端退出，fd = %d，当前客户端数 = %d\n", fd, count);
    broadcast_system(msg);
}



client_info_t *client_get(int fd)
{
    pthread_mutex_lock(&g_clients_lock);
    client_info_t *client = clients[fd];
    pthread_mutex_unlock(&g_clients_lock);
    return client;
}



int client_get_count()
{
    pthread_mutex_lock(&g_clients_lock);
    int count = client_count;
    pthread_mutex_unlock(&g_clients_lock);
    return count;
}



void client_set_nickname(int fd, const char *nick)
{
    pthread_mutex_lock(&g_clients_lock);
    if(clients[fd]->has_nickname)
    {
        pthread_mutex_unlock(&g_clients_lock);
        printf("昵称已存在\n");
        return;
    }
    if(strlen(nick) >= MAX_NICK_LEN)
    {
        pthread_mutex_unlock(&g_clients_lock);
        printf("昵称长度超过最大限制\n");
        return;
    }
    strncpy(clients[fd]->nickname, nick, MAX_NICK_LEN - 1);
    clients[fd]->nickname[MAX_NICK_LEN - 1] = '\0';
    clients[fd]->has_nickname = 1;
    pthread_mutex_unlock(&g_clients_lock);
    printf("fd = %d 昵称已设置，昵称 = %s\n", fd, nick);
    char msg[PROTOCOL_MAX_BODY_SIZE];
    snprintf(msg, sizeof(msg), "%s 加入了聊天室", nick);
    broadcast_system(msg);
}



void client_check_alive(void)
{
    int dead_fds[MAX_CLIENTS];
    int dead_errnos[MAX_CLIENTS];
    int dead_cnt = 0;

    pthread_mutex_lock(&g_clients_lock);
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
            dead_errnos[dead_cnt] = 0;// 对端已关闭（EOF），fd 还挂在表里 → 僵尸
            dead_fds[dead_cnt] = c->fd;
            dead_cnt++;
        }
        else if(ret < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 缓冲区空 = 连接健康空闲，跳过
            }
            else
            {
                dead_errnos[dead_cnt] = errno;
                dead_fds[dead_cnt] = c->fd;
                dead_cnt++;
            }
        }
        // ret > 0：有数据在排队，正常，主循环会处理
    }
    pthread_mutex_unlock(&g_clients_lock);
    
    for(int k = 0; k < dead_cnt; k++)
    {
        if(dead_errnos[k])
        {
            printf("[诊断] 巡检失败 fd=%d errno=%d\n", dead_fds[k], dead_errnos[k]);
        }
        client_remove(dead_fds[k]);
    }
}

int client_snapshot_fds(int *out, int max, int exclude_fd)
{
    pthread_mutex_lock(&g_clients_lock);
    int count = 0;
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        client_info_t *client = clients[i];
        if(client != NULL && client->has_nickname && i != exclude_fd && count < max)
        {
            out[count++] = client->fd;
        }
    }
    pthread_mutex_unlock(&g_clients_lock);
    return count;
}


int client_get_nickname(int fd, char *name, int namesize)
{
    pthread_mutex_lock(&g_clients_lock);
    if(fd <= 0 || fd >= MAX_CLIENTS || namesize <= 0 || clients[fd] == NULL)
    {
        pthread_mutex_unlock(&g_clients_lock);
        return -1;
    }
    if(clients[fd]->has_nickname)
    {
        strncpy(name, clients[fd]->nickname, namesize - 1);
        name[namesize - 1] = '\0';
    }
    else
    {
        pthread_mutex_unlock(&g_clients_lock);
        return -1;
    }
    pthread_mutex_unlock(&g_clients_lock);
    return 0;
}
