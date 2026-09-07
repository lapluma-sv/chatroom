#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

// 测试1：正常收发一条消息
void test_normal_echo(void)
{
    printf("[test1] normal echo... ");
    int fd = connect_server();

    uint8_t send_buf[PROTOCOL_BUF_SIZE];
    int len = protocol_pack(MSG_TEXT, "hello", send_buf, sizeof(send_buf));
    assert(len > 0);
    assert(write_exact(fd, send_buf, len) == len);

    uint8_t type;
    char msg[PROTOCOL_MAX_BODY_SIZE + 1];
    int ret = protocol_recv_msg(fd, &type, msg, sizeof(msg));
    assert(ret > 0);
    assert(type == MSG_TEXT);
    assert(strcmp(msg, "hello") == 0);

    close(fd);
    printf("PASS\n");
}

// 测试2：模拟拆包 - 分多次发送一个包
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

    uint8_t type;
    char msg[PROTOCOL_MAX_BODY_SIZE + 1];
    int ret = protocol_recv_msg(fd, &type, msg, sizeof(msg));
    assert(ret > 0);
    assert(type == MSG_TEXT);
    assert(strcmp(msg, "world") == 0);

    close(fd);
    printf("PASS\n");
}

// 测试3：模拟粘包 - 两个包一起发
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

    // 读第一个
    uint8_t type;
    char msg[PROTOCOL_MAX_BODY_SIZE + 1];
    int ret = protocol_recv_msg(fd, &type, msg, sizeof(msg));
    assert(ret > 0);
    assert(type == MSG_TEXT);
    assert(strcmp(msg, "hello") == 0);

    // 读第二个
    ret = protocol_recv_msg(fd, &type, msg, sizeof(msg));
    assert(ret > 0);
    assert(type == MSG_TEXT);
    assert(strcmp(msg, "world") == 0);

    close(fd);
    printf("PASS\n");
}

// 测试4：错误包 - 错误的魔数
void test_bad_magic(void)
{
    printf("[test4] bad magic... ");
    int fd = connect_server();

    uint8_t send_buf[PROTOCOL_BUF_SIZE];
    int len = protocol_pack(MSG_TEXT, "test", send_buf, sizeof(send_buf));
    assert(len > 0);

    // 篡改魔数
    send_buf[0] = 0xFF;

    assert(write_exact(fd, send_buf, len) == len);

    uint8_t type;
    char msg[PROTOCOL_MAX_BODY_SIZE + 1];
    int ret = protocol_recv_msg(fd, &type, msg, sizeof(msg));
    // 期望服务器拒绝错误包（关闭连接或返回错误）
    // 如果服务器正确处理，应该收到错误而不是正常回显
    if(ret > 0)
    {
        // 如果收到了响应，说明错误没被检测到
        printf("FAIL (server accepted bad magic)\n");
    }
    else
    {
        printf("PASS\n");
    }

    close(fd);
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

/* ---------- 主函数 ---------- */

int main(void)
{
    printf("=== Threadpool Test ===\n\n");

    // 等待服务器启动
    //sleep(1);

    client_manager_init(0);
    test_threadpool();

    printf("\n=== All Tests Done ===\n");
    return 0;
}
