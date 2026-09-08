#include "protocol.h"

/* ---------- 底层精确读写（解决 TCP 拆包） ---------- */

// 保证读取 n 字节，不够就循环收
int read_exact(int fd, void *buf, int n)
{
    int received = 0;
    while(received < n)
    {
        int ret = recv(fd, (char*)buf + received, n - received, 0);
        if(ret == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return PROTOCOL_ERR_WOULDBLOCK;
            }
            else if(errno == EINTR)
            {
                continue;
            }
            return -1;  //出错
        }
        if(ret == 0) return 0;
        received += ret;
    }
    return received;
}

// 保证发送 n 字节，没发完就循环发
int write_exact(int fd, const void *buf, int n)
{
    int sent = 0;
    while(sent < n)
    {
        int ret = send(fd, (const char*)buf + sent, n - sent, 0);
        if(ret == -1)
        {
            if(errno == EINTR)
            {
                continue;
            }
            return -1;  // 出错或被信号打断，直接返回
        }
        sent += ret;
    }
    return sent;
}

/* ---------- 打包/解包 ---------- */

// 打包：把消息类型和文本封装成协议包
int protocol_pack(uint8_t type, const char *msg, uint8_t *buf, int buf_len)
{
    if(msg == NULL || buf == NULL)
    {
        return PROTOCOL_ERR_INVALID;
    }
    int msg_len = strlen(msg);
    if(msg_len == 0)
    {
        return PROTOCOL_ERR_EMPTY;
    }
    if(msg_len > PROTOCOL_MAX_BODY_SIZE)
    {
        return PROTOCOL_ERR_MSGSIZE;
    }
    if(type == 0 || type > MSG_TYPE_COUNT)
    {
        return PROTOCOL_ERR_TYPE;
    }

    int total_len = PROTOCOL_HEADER_SIZE + msg_len + PROTOCOL_TAIL_SIZE;
    if(total_len > buf_len)
    {
        return PROTOCOL_ERR_BUFSIZE;
    }

    buf[0] = PROTOCOL_MAGIC;
    buf[1] = PROTOCOL_MAGIC;
    buf[2] = type;
    uint32_t body_len = htonl(msg_len);
    memcpy(buf + 3, &body_len, sizeof(body_len));

    memcpy(buf + PROTOCOL_HEADER_SIZE, msg, msg_len);

    uint8_t checksum = 0;
    for(int i = 0; i < msg_len; i++)
    {
        checksum ^= msg[i];
    }
    buf[total_len - 3] = checksum;
    buf[total_len - 2] = PROTOCOL_MAGIC;
    buf[total_len - 1] = PROTOCOL_MAGIC;
    return total_len;
}

// 解包：从协议包中解析出消息类型和文本
int protocol_unpack(const uint8_t *buf, int buf_len, uint8_t *type, char *msg, int msg_len)
{
    if(buf == NULL || msg == NULL)
    {
        return PROTOCOL_ERR_INVALID;
    }
    if(buf_len < PROTOCOL_HEADER_SIZE)
    {
        return PROTOCOL_ERR_MSGSIZE;
    }
    int body_len = ntohl(*(uint32_t*)(buf + 3));
    int total_len = PROTOCOL_HEADER_SIZE + body_len + PROTOCOL_TAIL_SIZE;
    if(body_len > PROTOCOL_MAX_BODY_SIZE)
    {
        return PROTOCOL_ERR_MSGSIZE;
    }
    if(buf_len < total_len)
    {
        return PROTOCOL_ERR_MSGSIZE;
    }
    if(buf[0] != PROTOCOL_MAGIC || buf[1] != PROTOCOL_MAGIC || buf[total_len - 2] != PROTOCOL_MAGIC || buf[total_len - 1] != PROTOCOL_MAGIC)
    {
        return PROTOCOL_ERR_MAGIC;
    }
    *type = buf[2];
    if(*type == 0 || *type > MSG_TYPE_COUNT)
    {
        return PROTOCOL_ERR_TYPE;
    }
    uint8_t checksum = 0;
    for(int i = 0; i < body_len; i++)
    {
        checksum ^= buf[PROTOCOL_HEADER_SIZE + i];
    }
    if(checksum != buf[total_len - 3])
    {
        return PROTOCOL_ERR_CHECKSUM;
    }
    if(msg_len < body_len + 1)
    {
        return PROTOCOL_ERR_MSGSIZE;
    }
    memcpy(msg, buf + PROTOCOL_HEADER_SIZE, body_len);
    msg[body_len] = '\0';
    return body_len;
}

/* ---------- 应用层协议读写（封装粘包处理） ---------- */

// 读取一个完整的应用层消息
// 返回: >0 成功, 0 客户端关闭, -1 出错
int protocol_recv_msg(int fd, uint8_t *type, char *msg, int msg_len)
{
    uint8_t header[PROTOCOL_HEADER_SIZE];

    // 1. 先读固定 7 字节包头
    int ret = read_exact(fd, header, PROTOCOL_HEADER_SIZE);
    if(ret != PROTOCOL_HEADER_SIZE) return ret;

    // 2. 从包头里读出正文长度
    int body_len = ntohl(*(uint32_t*)(header + 3));
    if(body_len > PROTOCOL_MAX_BODY_SIZE)
    {
        return PROTOCOL_ERR_MSGSIZE;
    }
    int total_len = PROTOCOL_HEADER_SIZE + body_len + PROTOCOL_TAIL_SIZE;

    // 3. 分配足够空间装整个包
    uint8_t *full_buf = malloc(total_len);
    if(!full_buf) return -1;
    memcpy(full_buf, header, PROTOCOL_HEADER_SIZE);

    // 4. 读出剩余的正文 + 尾部
    ret = read_exact(fd, full_buf + PROTOCOL_HEADER_SIZE, body_len + PROTOCOL_TAIL_SIZE);
    if(ret != body_len + PROTOCOL_TAIL_SIZE)
    {
        free(full_buf);
        return ret <= 0 ? ret : -1;
    }

    // 5. 解包
    ret = protocol_unpack(full_buf, total_len, type, msg, msg_len);
    free(full_buf);

    if(ret < 0) return ret;
    return body_len;
}

// 发送一个完整的应用层消息
int protocol_send_msg(int fd, uint8_t type, const char *msg)
{
    uint8_t buf[PROTOCOL_BUF_SIZE];

    // 1. 打包
    int total_len = protocol_pack(type, msg, buf, sizeof(buf));
    if(total_len < 0) return total_len;

    // 2. 完整发送
    return write_exact(fd, buf, total_len);
}

/* ---------- 工具函数 ---------- */

// 把 type 数字转成宏定义名称
const char* protocol_type_str(uint8_t type)
{
    switch(type)
    {
        case MSG_TEXT:      return "MSG_TEXT";
        case MSG_HEARTBEAT: return "MSG_HEARTBEAT";
        case MSG_QUIT:      return "MSG_QUIT";
        case MSG_NICKNAME:  return "MSG_NICKNAME";
        case MSG_SYSTEM:    return "MSG_SYSTEM";
        default:            return "UNKNOWN";
    }
}
