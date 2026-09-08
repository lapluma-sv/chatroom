#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <assert.h>
#include "protocol.h"
#include "threadpool.h"
#include "client_manager.h"

/* ---------- 测试工具 ---------- */

static int connect_server(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr("127.0.0.1"),
        .sin_port = htons(8888)
    };

    if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        close(fd);
        perror("connect");
        exit(1);
    }
    return fd;
}

/* ---------- 测试用例 ---------- */

// 测试1：正常收发一条消息（需要先启动服务器）
void test_normal_echo(void)
{
    printf("[test1] normal echo... ");
    int fd = connect_server();

    uint8_t send_buf[PROTOCOL_BUF_SIZE];
    int len = protocol_pack(MSG_TEXT, "hello", send_buf, sizeof(send_buf));
    assert(len > 0);
    assert(write_exact(fd, send_buf, len) == len);

    char recv_buf[PROTOCOL_BUF_SIZE];
    int recv_len = 0;
    uint8_t type;
    char msg[PROTOCOL_MAX_BODY_SIZE + 1];
    int ret = protocol_recv_msg(fd, recv_buf, &recv_len, &type, msg, sizeof(msg));
    assert(ret > 0);
    assert(type == MSG_TEXT);
    assert(strcmp(msg, "hello") == 0);

    close(fd);
    printf("PASS\n");
}

// 测试2：模拟拆包 - 分多次发送一个包（需要先启动服务器）
void test_fragmented_send(void)
{
    printf("[test2] fragmented send... ");
    int fd = connect_server();

    uint8_t send_buf[PROTOCOL_BUF_SIZE];
    int len = protocol_pack(MSG_TEXT, "world", send_buf, sizeof(send_buf));
    assert(len > 0);

    // 分3次发：包头 / 正文前半 / 正文后半+尾
    int part1 = 7;           // 包头
    int part2 = 3;           // 正文前3字节
    int part3 = len - part1 - part2;  // 剩余

    assert(write_exact(fd, send_buf, part1) == part1);
    usleep(10000);  // 10ms
    assert(write_exact(fd, send_buf + part1, part2) == part2);
    usleep(10000);
    assert(write_exact(fd, send_buf + part1 + part2, part3) == part3);

    char recv_buf[PROTOCOL_BUF_SIZE];
    int recv_len = 0;
    uint8_t type;
    char msg[PROTOCOL_MAX_BODY_SIZE + 1];
    int ret = protocol_recv_msg(fd, recv_buf, &recv_len, &type, msg, sizeof(msg));
    assert(ret > 0);
    assert(type == MSG_TEXT);
    assert(strcmp(msg, "world") == 0);

    close(fd);
    printf("PASS\n");
}

// 测试3：模拟粘包 - 两个包一起发（需要先启动服务器）
void test_combined_send(void)
{
    printf("[test3] combined send... ");
    int fd = connect_server();

    uint8_t buf1[PROTOCOL_BUF_SIZE];
    uint8_t buf2[PROTOCOL_BUF_SIZE];
    int len1 = protocol_pack(MSG_TEXT, "hello", buf1, sizeof(buf1));
    int len2 = protocol_pack(MSG_TEXT, "world", buf2, sizeof(buf2));
    assert(len1 > 0 && len2 > 0);

    // 两个包拼在一起
    uint8_t combined[PROTOCOL_BUF_SIZE * 2];
    memcpy(combined, buf1, len1);
    memcpy(combined + len1, buf2, len2);

    assert(write_exact(fd, combined, len1 + len2) == len1 + len2);

    char recv_buf[PROTOCOL_BUF_SIZE];
    int recv_len = 0;
    uint8_t type;
    char msg[PROTOCOL_MAX_BODY_SIZE + 1];

    // 读第一个
    int ret = protocol_recv_msg(fd, recv_buf, &recv_len, &type, msg, sizeof(msg));
    assert(ret > 0);
    assert(type == MSG_TEXT);
    assert(strcmp(msg, "hello") == 0);

    // 读第二个（内核已空，第二帧留在 recv_buf 里，状态机应直接从缓冲区解析出来）
    ret = protocol_recv_msg(fd, recv_buf, &recv_len, &type, msg, sizeof(msg));
    assert(ret > 0);
    assert(type == MSG_TEXT);
    assert(strcmp(msg, "world") == 0);

    close(fd);
    printf("PASS\n");
}

void test_threadpool(void)
{
    threadpool_t *tp = threadpool_create(4);
    assert(tp != NULL);
    for(int i = 0; i < 100; i++)
    {
        char buf[PROTOCOL_MAX_BODY_SIZE + 1];
        snprintf(buf, sizeof(buf), "task %d", i);
        int len = strlen(buf);
        int ret = threadpool_submit(tp, i, MSG_TEXT, buf, len);
        assert(ret == 0);
    }
    printf("all submitted\n");
    threadpool_destroy(tp);
}

// 测试5：半包验证 —— 前半包遇 EAGAIN 后，已读字节必须保留在 recv_buf 里
// 不依赖服务器，用 socketpair 造一对互联的非阻塞 socket
void test_half_packet(void)
{
    printf("[test5] half packet verify...\n");
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    // 两端都设非阻塞，模拟服务器里的 client socket
    for(int i = 0; i < 2; i++)
    {
        int flags = fcntl(sv[i], F_GETFL, 0);
        fcntl(sv[i], F_SETFL, flags | O_NONBLOCK);
    }

    uint8_t send_buf[PROTOCOL_BUF_SIZE];
    int total_len = protocol_pack(MSG_TEXT, "hello", send_buf, sizeof(send_buf));
    assert(total_len > 0);

    // 第一步：只发前 3 字节（magic + magic + type）
    assert(send(sv[0], send_buf, 3, 0) == 3);

    char recv_buf[PROTOCOL_BUF_SIZE];
    int recv_len = 0;
    uint8_t type;
    char msg[PROTOCOL_MAX_BODY_SIZE + 1];
    int ret = protocol_recv_msg(sv[1], recv_buf, &recv_len, &type, msg, sizeof(msg));
    printf("  第一次 recv_msg 返回 = %d（期望 %d = PROTOCOL_ERR_WOULDBLOCK）\n",
           ret, PROTOCOL_ERR_WOULDBLOCK);
    assert(ret == PROTOCOL_ERR_WOULDBLOCK);
    assert(recv_len == 3);   // 半包字节必须留在缓冲区，不能丢

    // 第二步：补发剩余字节
    assert(send(sv[0], send_buf + 3, total_len - 3, 0) == total_len - 3);

    // 第三步：剩余部分到齐，状态机应从 recv_buf 拼出完整帧并解析
    ret = protocol_recv_msg(sv[1], recv_buf, &recv_len, &type, msg, sizeof(msg));
    if(ret > 0 && type == MSG_TEXT && strcmp(msg, "hello") == 0 && recv_len == 0)
    {
        printf("  第二次 recv_msg 返回 = %d, msg = \"%s\"\n", ret, msg);
        printf("  PASS：半包数据被正确保留并解析（状态机工作正常）\n");
    }
    else
    {
        printf("  第二次 recv_msg 返回 = %d, msg = \"%s\", recv_len = %d\n",
               ret, ret > 0 ? msg : "(nil)", recv_len);
        printf("  FAIL：半包状态未正确恢复\n");
    }

    close(sv[0]);
    close(sv[1]);
}

/* ---------- 主函数 ---------- */

int main(void)
{
    printf("\n=== Half Packet Test ===\n\n");
    test_half_packet();

    printf("\n=== All Tests Done ===\n");
    return 0;
}
