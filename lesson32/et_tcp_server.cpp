
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unordered_map>
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
    // 避免客户端异常断开时，SIGPIPE 直接终止服务器
    signal(SIGPIPE, SIG_IGN);

    // 1. 创建监听 Socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenfd == -1)
    {
        perror("socket");
        return 1;
    }

    // 2. 设置端口重用
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

    // 4. 开始监听
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

    // 7. 将监听 Socket 注册到 epoll
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

    // 8. 保存每个客户端尚未发送完的数据
    unordered_map<int, string> pending;

    // 9. epoll 事件数组
    epoll_event events[16]{};

    // 10. 发送指定客户端尚未发送完的数据
    auto flushPending = [&](int fd) -> bool
    {
        auto it = pending.find(fd);

        if (it == pending.end())
        {
            return true;
        }

        string& out = it->second;

        while (!out.empty())
        {
            ssize_t n = write(fd, out.data(), out.size());

            if (n > 0)
            {
                // 删除已经成功写入的部分
                out.erase(0, static_cast<size_t>(n));
                continue;
            }

            if (n == -1 && errno == EINTR)
            {
                continue;
            }

            if (n == -1 &&
                (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                // 诊断信息：记录第一次遇到 EAGAIN
                static bool printedEagain = false;

                if (!printedEagain)
                {
                    cout << "write reached EAGAIN; pending = "
                         << out.size() << " bytes" << endl;

                    printedEagain = true;
                }

                // 保留剩余数据，等待后续可写事件
                break;
            }

            if (n == 0)
            {
                cerr << "write returned 0" << endl;
            }
            else
            {
                perror("write");
            }

            return false;
        }

        // 判断是否仍有数据等待发送
        bool hasPending = !out.empty();

        if (!hasPending)
        {
            pending.erase(it);
        }

        // 有剩余数据才关注 EPOLLOUT
        epoll_event clientEvent{};
        clientEvent.events = EPOLLIN | EPOLLET;

        if (hasPending)
        {
            clientEvent.events |= EPOLLOUT;
        }

        clientEvent.data.fd = fd;

        if (epoll_ctl(epfd, EPOLL_CTL_MOD,
                      fd, &clientEvent) == -1)
        {
            perror("epoll_ctl MOD");
            return false;
        }

        return true;
    };

    // 11. 主事件循环
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

        for (int i = 0; i < nready; ++i)
        {
            int fd = events[i].data.fd;
            uint32_t eventFlags = events[i].events;

            // 情况一：监听 Socket 有新连接
            if (fd == listenfd)
            {
                // ET 模式下循环接受连接，直到 EAGAIN
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

                    // 注册客户端连接，暂时只关注可读事件
                    epoll_event clientEvent{};
                    clientEvent.events = EPOLLIN | EPOLLET;
                    clientEvent.data.fd = connfd;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD,
                                  connfd, &clientEvent) == -1)
                    {
                        perror("epoll_ctl ADD connfd");
                        close(connfd);
                        continue;
                    }

                    cout << "Client connected, fd = "
                         << connfd << endl;
                }
            }
            // 情况二：处理已有客户端
            else
            {
                bool shouldClose = false;

                // 12. 客户端再次可写，尝试发送剩余数据
                if (eventFlags & EPOLLOUT)
                {
                    cout << "EPOLLOUT: fd = " << fd
                         << " is writable again" << endl;

                    if (!flushPending(fd))
                    {
                        shouldClose = true;
                    }
                }

                // 13. 客户端有数据可读，或连接状态发生变化
                if (!shouldClose &&
                    (eventFlags & (EPOLLIN | EPOLLHUP | EPOLLERR)))
                {
                    while (true)
                    {
                        char buffer[1024];

                        ssize_t n = read(fd, buffer, sizeof(buffer));

                        if (n > 0)
                        {
                            // 保存收到的数据，准备回传给客户端
                            pending[fd].append(
                                buffer,
                                static_cast<size_t>(n)
                            );

                            // 尝试发送；如果遇到 EAGAIN，
                            // flushPending 会保留剩余数据
                            if (!flushPending(fd))
                            {
                                shouldClose = true;
                                break;
                            }

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
                            break;
                        }

                        perror("read");
                        shouldClose = true;
                        break;
                    }
                }

                // 对端挂断或发生错误时，清理连接
                if (eventFlags & (EPOLLHUP | EPOLLERR))
                {
                    shouldClose = true;
                }

                if (shouldClose)
                {
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL,
                                  fd, nullptr) == -1)
                    {
                        perror("epoll_ctl DEL");
                    }

                    pending.erase(fd);
                    close(fd);
                }
            }
        }
    }

    // 14. 释放服务器资源
    close(epfd);
    close(listenfd);

    return 0;
}
