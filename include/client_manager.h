#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "protocol.h"
#include "broadcast.h"
#include <sys/epoll.h>
#include <pthread.h>

#define MAX_CLIENTS 1024
#define MAX_NICK_LEN 32

typedef struct {
    int fd;
    char nickname[MAX_NICK_LEN];    
    int has_nickname;               
} client_info_t;

int client_manager_init(int fd); // 初始化客户端管理器
void client_join(int fd); // 新客户端加入
void client_remove(int fd); // 客户端移除
client_info_t *client_get(int fd); // 根据 fd 获取客户端信息（供广播模块用）
int client_get_count(); // 获取在线人数
void client_set_nickname(int fd, const char *nick); // 设置客户端昵称
void client_check_alive(void); // 巡检：清理 fd 已死但记录未删的僵尸客户端
int client_snapshot_fds(int *out, int max, int exclude_fd); // 获取所有在线客户端 fd（排除 exclude_fd）

#endif