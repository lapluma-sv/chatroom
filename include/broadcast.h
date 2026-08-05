#ifndef BROADCAST_H
#define BROADCAST_H

#include "protocol.h"
#include "client_manager.h"
#include <stdio.h>
#include <string.h>

void broadcast_system(const char *msg);
void broadcast_text(int sender_fd, const char *msg);

#endif