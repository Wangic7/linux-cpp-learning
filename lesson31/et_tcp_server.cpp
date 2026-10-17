
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

// 将文件描述符设置为非阻塞模式
bool setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL");
        return false;
    }

    return true;
}

int main()
{
    // 1. 创建监听 Socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenfd == -1)
    {
        perror("socket");
        return 1;
    }

    // 2. 允许端口快速重用
    int reuse = 1;

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) == -1)
    {
        perror("setsockopt");
        close(listenfd);
        return 1;
    }

    // 3. 绑定 IP 地址和端口
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(listenfd,
             reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) == -1)
    {
        perror("bind");
        close(listenfd);
        return 1;
    }

    // 4. 开始监听客户端连接
    if (listen(listenfd, SOMAXCONN) == -1)
    {
        perror("listen");
        close(listenfd);
        return 1;
    }

    // 5. 监听 Socket 设置为非阻塞
    if (!setNonBlocking(listenfd))
    {
        close(listenfd);
        return 1;
    }

    // 6. 创建 epoll 实例
    int epfd = epoll_create1(0);

    if (epfd == -1)
    {
        perror("epoll_create1");
        close(listenfd);
        return 1;
    }

    // 7. 将监听 Socket 注册到 epoll，使用 ET 模式
    epoll_event listenEvent{};
    listenEvent.events = EPOLLIN | EPOLLET;
    listenEvent.data.fd = listenfd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD,
                  listenfd, &listenEvent) == -1)
    {
        perror("epoll_ctl listenfd");
        close(epfd);
        close(listenfd);
        return 1;
    }

    cout << "ET TCP server listening on port 8080" << endl;

    // 8. 事件数组，只声明一次
    epoll_event events[16]{};

    // 9. 主事件循环
    while (true)
    {
        int nready = epoll_wait(epfd, events, 16, -1);

        if (nready == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("epoll_wait");
            break;
        }

        // 处理本次返回的所有事件
        for (int i = 0; i < nready; ++i)
        {
            int fd = events[i].data.fd;

            // 情况一：监听 Socket 有新连接
            if (fd == listenfd)
            {
                // ET 模式下，循环接受所有待处理连接
                while (true)
                {
                    sockaddr_in clientAddr{};
                    socklen_t clientLen = sizeof(clientAddr);

                    int connfd = accept(
                        listenfd,
                        reinterpret_cast<sockaddr*>(&clientAddr),
                        &clientLen
                    );

                    if (connfd == -1)
                    {
                        if (errno == EINTR)
                        {
                            continue;
                        }

                        if (errno == EAGAIN ||
                            errno == EWOULDBLOCK)
                        {
                            // 当前没有更多连接可接受
                            break;
                        }

                        perror("accept");
                        break;
                    }

                    // 新连接也设置为非阻塞
                    if (!setNonBlocking(connfd))
                    {
                        close(connfd);
                        continue;
                    }

                    // 将客户端 Socket 注册到 epoll
                    epoll_event clientEvent{};
                    clientEvent.events = EPOLLIN | EPOLLET;
                    clientEvent.data.fd = connfd;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD,
                                  connfd, &clientEvent) == -1)
                    {
                        perror("epoll_ctl connfd");
                        close(connfd);
                        continue;
                    }

                    cout << "Client connected, fd = "
                         << connfd << endl;
                }
            }
            // 情况二：已有客户端连接发生事件
            else
            {
                bool shouldClose = false;

                // ET 模式下，循环读取直到没有更多数据
                while (true)
                {
                    char buffer[1024];

                    ssize_t n = read(fd, buffer, sizeof(buffer));

                    if (n > 0)
                    {
                        cout << "Received from fd "
                             << fd << ": ";

                        cout.write(buffer,
                                   static_cast<streamsize>(n));
                        cout << endl;

                        continue;
                    }

                    if (n == 0)
                    {
                        cout << "Client disconnected, fd = "
                             << fd << endl;

                        shouldClose = true;
                        break;
                    }

                    if (errno == EINTR)
                    {
                        continue;
                    }

                    if (errno == EAGAIN ||
                        errno == EWOULDBLOCK)
                    {
                        // 当前数据已读取完毕
                        break;
                    }

                    perror("read");
                    shouldClose = true;
                    break;
                }

                // 客户端断开或发生读取错误时，释放连接
                if (shouldClose)
                {
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL,
                                  fd, nullptr) == -1)
                    {
                        perror("epoll_ctl DEL");
                    }

                    close(fd);
                }
            }
        }
    }

    // 10. 释放服务器资源
    close(epfd);
    close(listenfd);

    return 0;
}
