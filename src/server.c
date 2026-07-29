#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

volatile sig_atomic_t running = 1;

void sigint_handler(int signum)
{
    (void)signum;
    running = 0;
}

int main(void)
{
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  // ← 关键：不设置 SA_RESTART
    sigaction(SIGINT, &sa, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) 
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;      // 监听所有网卡
    addr.sin_port = htons(8888);             // 端口号（网络字节序）
    if(bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) 
    {
        close(server_fd);
        perror("bind");
        exit(1);
    }

    // 3. 开始监听
    if(listen(server_fd, 5) == -1) 
    {
        close(server_fd);
        perror("listen");
        exit(1);
    }
    printf("server start.\n");
    while(running)
    {
        // 4. 接受连接
        int client_fd = accept(server_fd, NULL, NULL);
        if(client_fd < 0)
        {
            if(errno != EINTR)
            {
                perror("accept");
            }
            break;
        }
        printf("server accept client.\n");

        // 5. 循环接收和发送
        char buf[1024];
        while (1) 
        {
            int len = recv(client_fd, buf, sizeof(buf) - 1, 0);
            if(len < 0)    
            {
                if(errno != EINTR)
                {
                    perror("recv");
                }
                break;
            }
            else if(len == 0)
            {
                printf("client quit.\n");
                break;
            }
            buf[len] = '\0';
            printf("server receive : %s", buf);
            if(send(client_fd, buf, strlen(buf), 0) == -1)
            {
                perror("send");
                break;
            }
            else
            {
                printf("server send : %s", buf);
            }
        }
        // 6. 关闭连接
        close(client_fd);
        printf("client exit.\n");
    }
    close(server_fd);
    printf("server close.\n");
    return 0;
}