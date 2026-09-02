#include "broadcast.h"

void broadcast_system(const char *msg)
{
    char buf[PROTOCOL_MAX_BODY_SIZE];
    snprintf(buf, sizeof(buf), "*** %s", msg);


    int dead_fds[MAX_CLIENTS];
    int dead_fd_cnt = 0;


    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        client_info_t *client = client_get(i);
        if(client != NULL && client->has_nickname)
        {
            if(protocol_send_msg(client->fd, MSG_SYSTEM, buf) < 0)
            {
                dead_fds[dead_fd_cnt++] = client->fd;
            }
        }
    }

    for(int i = 0; i < dead_fd_cnt; i++)
    {
        client_remove(dead_fds[i]);
    }
}



void broadcast_text(int sender_fd, const char *msg)
{
    char buf[PROTOCOL_MAX_BODY_SIZE];
    client_info_t *sender = client_get(sender_fd);
    if (sender == NULL) 
    {
        return;
    }
    snprintf(buf, sizeof(buf), "%s: %s", sender->nickname, msg);

    int dead_fds[MAX_CLIENTS];
    int dead_fd_cnt = 0;

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        client_info_t *client = client_get(i);
        if(client != NULL && client->has_nickname && i != sender_fd)
        {
            if(protocol_send_msg(client->fd, MSG_TEXT, buf) < 0)
            {
                printf("[诊断] 广播失败 fd=%d errno=%d(%s)\n",
                       client->fd, errno, strerror(errno));
                dead_fds[dead_fd_cnt++] = client->fd;
            }
        }
    }

    for(int i = 0; i < dead_fd_cnt; i++)
    {
        client_remove(dead_fds[i]);
    }
}