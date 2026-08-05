#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

/* 协议常量 */
#define PROTOCOL_MAGIC     0xA5
#define PROTOCOL_HEADER_SIZE   7
#define PROTOCOL_MAX_BODY_SIZE 1024
#define PROTOCOL_TAIL_SIZE     3
#define PROTOCOL_BUF_SIZE      (PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_BODY_SIZE + PROTOCOL_TAIL_SIZE)

/* 消息类型 */
#define MSG_TYPE_COUNT 5
#define MSG_TEXT      0x01
#define MSG_HEARTBEAT 0x02
#define MSG_QUIT      0x03
#define MSG_NICKNAME  0x04
#define MSG_SYSTEM    0x05

/* 错误码 */
#define PROTOCOL_ERR_EMPTY      0
#define PROTOCOL_ERR_INVALID    -1
#define PROTOCOL_ERR_MSGSIZE    -2
#define PROTOCOL_ERR_TYPE       -3
#define PROTOCOL_ERR_UNKNOWN    -4
#define PROTOCOL_ERR_MAGIC      -5
#define PROTOCOL_ERR_CHECKSUM   -6
#define PROTOCOL_ERR_BUFSIZE   -7


/* 函数声明 */
int protocol_pack(uint8_t type, const char *msg, uint8_t *buf, int buf_len);
int protocol_unpack(const uint8_t *buf, int buf_len, uint8_t *type, char *msg, int msg_len);
int read_exact(int fd, void *buf, int n);
int write_exact(int fd, const void *buf, int n);
int protocol_recv_msg(int fd, uint8_t *type, char *msg, int msg_len);
int protocol_send_msg(int fd, uint8_t type, const char *msg);
const char* protocol_type_str(uint8_t type);

#endif