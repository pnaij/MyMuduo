//
// Created by jianp on 2026/3/13.
//

// test_client_optimized.cpp
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>

#define MAX_CONNS 10000
#define EPOLL_SIZE 10000
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8000

// 设置非阻塞socket
int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 提升进程fd上限
void set_fd_limit() {
    struct rlimit rlim;
    rlim.rlim_cur = 1000000;
    rlim.rlim_max = 1000000;
    setrlimit(RLIMIT_NOFILE, &rlim);
}

int main() {
    set_fd_limit(); // 提升进程fd上限
    int epollfd = epoll_create1(EPOLL_CLOEXEC);
    epoll_event events[EPOLL_SIZE];
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    int fds[MAX_CONNS] = {0}; // 存储所有socket fd
    int create_cnt = 0;
    int retry_cnt = 0;

    // 循环创建连接，直到达到MAX_CONNS或无端口可用
    while (create_cnt < MAX_CONNS) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            if (errno == EMFILE || errno == ENFILE) {
                // fd/端口耗尽，短暂休眠后重试
                usleep(1000);
                retry_cnt++;
                if (retry_cnt > 1000) break; // 重试1000次仍失败则退出
                continue;
            }
            perror("socket fail");
            continue;
        }
        set_nonblock(sockfd);
        connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

        // 添加到epoll监控连接成功事件
        epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;
        ev.data.fd = sockfd;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);

        fds[create_cnt++] = sockfd;
        retry_cnt = 0; // 重置重试计数器

        // 每创建1000个连接，休眠5ms避免端口耗尽
        if (create_cnt % 1000 == 0) {
            printf("Created %d connections\n", create_cnt);
            usleep(5000);
        }
    }

    // 统计成功连接数
    int success = 0;
    int timeout = 10000; // 总等待时间10秒
    int interval = 100;  // 每次epoll_wait等待100ms
    while (timeout > 0 && create_cnt > 0) {
        int n = epoll_wait(epollfd, events, EPOLL_SIZE, interval);
        timeout -= interval;
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (events[i].events & EPOLLOUT) {
                // 验证连接是否真的成功（非阻塞connect的关键）
                int err = 0;
                socklen_t err_len = sizeof(err);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_len);
                if (err == 0) {
                    success++;
                }
                // 移除epoll监控，避免重复统计
                epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
            } else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
            }
        }
    }

    printf("Successfully established: %d/%d connections\n", success, MAX_CONNS);
    printf("Press Ctrl+C to exit...\n");
    while (1) sleep(1);

    // 清理资源（实际按Ctrl+C退出，此处仅为规范）
    for (int i = 0; i < create_cnt; ++i) {
        close(fds[i]);
    }
    close(epollfd);
    return 0;
}