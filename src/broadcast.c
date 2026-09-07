#include "broadcast.h"

static int g_expected_disconnected = 0;

int is_expected_disconnected(int err)
{
    return err == EPIPE || err == ECONNRESET;
}

static void broadcast_common(int sender_fd, int type, const char *body)
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

    for(int i = 0; i < fds_cnt; i++)
    {
        int ret = protocol_send_msg(fds[i], type, msg);
        if(ret < 0)
        {
            int err = errno;
            if(is_expected_disconnected(err))
            {
                __atomic_fetch_add(&g_expected_disconnected, 1, __ATOMIC_RELAXED);
            }
            else
            {
                printf("[诊断] 广播失败 fd=%d errno=%d(%s)\n", fds[i], err, strerror(err));
            }
            dead_fds[dead_cnt++] = fds[i];
        }
    }
    for(int i = 0; i < dead_cnt; i++)
    {
        client_remove(dead_fds[i]);
    }
}

void broadcast_system(const char *msg)
{
    broadcast_common(-1, MSG_SYSTEM, msg);
}



void broadcast_text(int sender_fd, const char *msg)
{
    char buf[PROTOCOL_MAX_BODY_SIZE];
    char name[MAX_NICK_LEN] = "";
    if(client_get_nickname(sender_fd, name, sizeof(name)) < 0)
    {
        return;
    }
    snprintf(buf, sizeof(buf), "%s: %s", name, msg);
    broadcast_common(sender_fd, MSG_TEXT, buf);
}

int get_expected_disconnected(void)
{
    return __atomic_load_n(&g_expected_disconnected, __ATOMIC_RELAXED);
}
