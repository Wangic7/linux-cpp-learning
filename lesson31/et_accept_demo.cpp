
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

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
    // 1. 创建监听套接字
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenfd == -1)
    {
        perror("socket");
        return 1;
    }

    int reuse = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) == -1)
    {
        perror("setsockopt");
        close(listenfd);
        return 1;
    }

    // 2. 绑定地址和端口
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(listenfd, reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) == -1)
    {
        perror("bind");
        close(listenfd);
        return 1;
    }

    // 3. 开始监听
    if (listen(listenfd, SOMAXCONN) == -1)
    {
        perror("listen");
        close(listenfd);
        return 1;
    }

    // 4. 将监听套接字设置为非阻塞
    if (!setNonBlocking(listenfd))
    {
        close(listenfd);
        return 1;
    }

    // 5. 创建 epoll 实例
    int epfd = epoll_create1(0);

    if (epfd == -1)
    {
        perror("epoll_create1");
        close(listenfd);
        return 1;
    }

    // 6. 将 listenfd 注册为 ET 模式
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenfd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1)
    {
        perror("epoll_ctl");
        close(epfd);
        close(listenfd);
        return 1;
    }

    cout << "ET accept demo listening on port 8080" << endl;

    epoll_event events[16]{};
    bool running = true;

    // 7. 事件循环
    while (running)
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

        for (int i = 0; i < nready; ++i)
        {
            if (events[i].data.fd != listenfd)
            {
                continue;
            }

            // 8. ET 模式：循环接受连接，直到 EAGAIN
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

                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        break;
                    }

                    perror("accept");
                    running = false;
                    break;
                }

                cout << "Accepted connection, connfd = "
                     << connfd << endl;

                // 本阶段只验证 accept 循环，暂时关闭客户端连接
                close(connfd);
            }

            if (!running)
            {
                break;
            }
        }
    }

    close(epfd);
    close(listenfd);

    return 0;
}
