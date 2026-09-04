#include "broadcast.h"

static int broadcast_common(int sender_fd, int type, const char *body)
{
    const char *msg = body;
    char buf[PROTOCOL_MAX_BODY_SIZE];
    if(type == MSG_SYSTEM)
    {
        snprintf(buf, sizeof(buf), "*** %s", msg);
        msg = buf;
    }

    int fds[MAX_CLIENTS];
    int fds_cnt = client_snapshot_fds(fds, MAX_CLIENTS, sender_fd);
    int dead_fds[MAX_CLIENTS];
    int dead_cnt = 0;
    int delivered = 0;

    for(int i = 0; i < fds_cnt; i++)
    {
        int ret = protocol_send_msg(fds[i], type, msg);
        if(ret < 0)
        {
            printf("[诊断] 广播失败 fd=%d errno=%d(%s)\n", fds[i], errno, strerror(errno));
            dead_fds[dead_cnt++] = fds[i];
        }
        else
        {
            delivered++;
        }
    }
    for(int i = 0; i < dead_cnt; i++)
    {
        client_remove(dead_fds[i]);
    }
    return delivered;
}

void broadcast_system(const char *msg)
{
    broadcast_common(-1, MSG_SYSTEM, msg);
}



void broadcast_text(int sender_fd, const char *msg)
{
    char buf[PROTOCOL_MAX_BODY_SIZE];
    client_info_t *sender = client_get(sender_fd);// TODO(step3): 指针生命周期
    if (sender == NULL) 
    {
        return;
    }
    snprintf(buf, sizeof(buf), "%s: %s", sender->nickname, msg);
    broadcast_common(sender_fd, MSG_TEXT, buf);
}