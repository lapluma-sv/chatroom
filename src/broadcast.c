#include "broadcast.h"

void broadcast_system(const char *msg)
{
    char buf[PROTOCOL_MAX_BODY_SIZE];
    snprintf(buf, sizeof(buf), "*** %s", msg);
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        client_info_t *client = client_get(i);
        if(client != NULL && client->has_nickname)
        {
            protocol_send_msg(client->fd, MSG_SYSTEM, buf);
        }
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
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        client_info_t *client = client_get(i);
        if(client != NULL && client->has_nickname && i != sender_fd)
        {
            protocol_send_msg(client->fd, MSG_TEXT, buf);
        }
    }
}