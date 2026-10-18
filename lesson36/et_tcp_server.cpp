
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

// 保存每个客户端独立的通信状态
struct ClientState
{
    string pending;                 // 尚未发送完的数据
    bool peerWriteClosed = false;  // 对端是否已关闭发送方向
    string recvBuffer;              // 尚未组成完整消息的数据
};

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
    // 避免向已断开的连接写入时，SIGPIPE 直接终止进程
    signal(SIGPIPE, SIG_IGN);

    // 1. 创建监听 Socket
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

    if (bind(listenfd,
             reinterpret_cast<sockaddr*>(&addr),
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

    if (!setNonBlocking(listenfd))
    {
        close(listenfd);
        return 1;
    }

    // 4. 创建 epoll 实例
    int epfd = epoll_create1(0);

    if (epfd == -1)
    {
        perror("epoll_create1");
        close(listenfd);
        return 1;
    }

    // 注册监听 Socket
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

    cout << "Lesson 36 TCP server listening on port 8080"
         << endl;

    // 保存所有客户端的独立状态
    unordered_map<int, ClientState> clients;

    epoll_event events[16]{};

    // 5. 关闭客户端并释放其状态
    auto closeClient = [&](int fd)
    {
        if (epoll_ctl(epfd, EPOLL_CTL_DEL,
                      fd, nullptr) == -1 &&
            errno != ENOENT)
        {
            perror("epoll_ctl DEL");
        }

        clients.erase(fd);

        if (close(fd) == -1)
        {
            perror("close client");
        }

        cout << "Closing client, fd = " << fd << endl;
    };

    // 6. 尝试发送指定客户端的待发送数据
    auto flushPending = [&](int fd) -> bool
    {
        auto it = clients.find(fd);

        if (it == clients.end())
        {
            return false;
        }

        ClientState& state = it->second;

        while (!state.pending.empty())
        {
            ssize_t n = write(
                fd,
                state.pending.data(),
                state.pending.size()
            );

            if (n > 0)
            {
                // 删除已经成功写入 Socket 的数据
                state.pending.erase(
                    0, static_cast<size_t>(n)
                );

                continue;
            }

            if (n == -1 && errno == EINTR)
            {
                continue;
            }

            if (n == -1 &&
                (errno == EAGAIN ||
                 errno == EWOULDBLOCK))
            {
                static bool printedEagain = false;

                if (!printedEagain)
                {
                    cout << "write reached EAGAIN; pending = "
                         << state.pending.size()
                         << " bytes" << endl;

                    printedEagain = true;
                }

                // 暂时无法继续发送，保留剩余数据
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

        // 对端已停止发送，且服务器没有待发送数据
        // 由调用者决定关闭连接
        if (state.peerWriteClosed &&
            state.pending.empty())
        {
            return true;
        }

        // 根据当前状态重新设置监听事件
        epoll_event clientEvent{};
        clientEvent.events = EPOLLET;

        // 对端仍能发送时，继续关注输入及半关闭事件
        if (!state.peerWriteClosed)
        {
            clientEvent.events |= EPOLLIN | EPOLLRDHUP;
        }

        // 只有存在待发送数据时，才关注可写事件
        if (!state.pending.empty())
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

    // 7. 主事件循环
    while (true)
    {
        int nready = epoll_wait(
            epfd, events, 16, -1
        );

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
            uint32_t flags = events[i].events;

            // 8. 处理监听 Socket：接受新连接
            if (fd == listenfd)
            {
                while (true)
                {
                    sockaddr_in clientAddr{};
                    socklen_t clientLen =
                        sizeof(clientAddr);

                    int connfd = accept(
                        listenfd,
                        reinterpret_cast<sockaddr*>(
                            &clientAddr
                        ),
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

                    // 新客户端也必须设置为非阻塞
                    if (!setNonBlocking(connfd))
                    {
                        close(connfd);
                        continue;
                    }

                    // 先创建独立客户端状态
                    clients.emplace(
                        connfd, ClientState{}
                    );

                    epoll_event clientEvent{};
                    clientEvent.events =
                        EPOLLIN | EPOLLRDHUP | EPOLLET;
                    clientEvent.data.fd = connfd;

                    if (epoll_ctl(
                            epfd, EPOLL_CTL_ADD,
                            connfd, &clientEvent
                        ) == -1)
                    {
                        perror("epoll_ctl ADD");
                        clients.erase(connfd);
                        close(connfd);
                        continue;
                    }

                    cout << "Client connected, fd = "
                         << connfd << endl;
                }

                continue;
            }

            // 9. 查找已有客户端
            auto it = clients.find(fd);

            if (it == clients.end())
            {
                continue;
            }

            bool shouldClose = false;

            // 10. 处理 Socket 错误
            if (flags & EPOLLERR)
            {
                int socketError = 0;
                socklen_t errorLen =
                    sizeof(socketError);

                if (getsockopt(
                        fd, SOL_SOCKET, SO_ERROR,
                        &socketError, &errorLen
                    ) == -1)
                {
                    perror("getsockopt SO_ERROR");
                }
                else
                {
                    cerr << "EPOLLERR: fd = " << fd
                         << ", SO_ERROR = "
                         << socketError << endl;
                }

                // 当前课程采用保守策略：清理错误连接
                shouldClose = true;
            }

            // 11. Socket 再次可写，继续发送
            if (!shouldClose && (flags & EPOLLOUT))
            {
                cout << "EPOLLOUT: fd = " << fd
                     << " is writable again" << endl;

                if (!flushPending(fd))
                {
                    shouldClose = true;
                }
                else
                {
                    auto current = clients.find(fd);

                    if (current != clients.end() &&
                        current->second.peerWriteClosed &&
                        current->second.pending.empty())
                    {
                        shouldClose = true;
                    }
                }
            }

            // 12. 处理可读、半关闭及挂断事件
            if (!shouldClose &&
                (flags & (EPOLLIN |
                          EPOLLRDHUP |
                          EPOLLHUP)))
            {
                while (true)
                {
                    char buffer[1024];

                    ssize_t n = read(
                        fd, buffer, sizeof(buffer)
                    );

                    if (n > 0)
                    {
                        auto current = clients.find(fd);

                        if (current == clients.end())
                        {
                            shouldClose = true;
                            break;
                        }

                        // 保存刚刚读取到的数据
                        current->second.recvBuffer.append(
                            buffer, static_cast<size_t>(n)
                        );

                        // 从接收缓冲区提取完整消息
                        while (true)
                        {
                            size_t pos =
                                current->second.recvBuffer.find(
                                    '\n'
                                );

                            // 没有换行符，暂时没有完整消息
                            if (pos == string::npos)
                            {
                                break;
                            }

                            // 提取一条完整消息，不包含换行符
                            string message =
                                current->second.recvBuffer.substr(
                                    0, pos
                                );

                            cout << "Complete message from fd "
                                 << fd
                                 << ", length = "
                                 << message.size()
                                 << " bytes" << endl;

                            // 将完整消息加入待发送缓冲区
                            current->second.pending.append(
                                message
                            );

                            current->second.pending.push_back(
                                '\n'
                            );

                            // 删除已经处理的消息
                            current->second.recvBuffer.erase(
                                0, pos + 1
                            );
                        }

                        cout << "Received " << n
                             << " bytes from fd "
                             << fd << endl;

                        if (!flushPending(fd))
                        {
                            shouldClose = true;
                            break;
                        }

                        // ET 模式继续读取，直到 EAGAIN 等条件
                        continue;
                    }

                    if (n == 0)
                    {
                        auto current = clients.find(fd);

                        if (current != clients.end())
                        {
                            current->second.peerWriteClosed =
                                true;

                            // EOF 时，未以换行符结束的数据
                            // 在当前协议中视为不完整消息
                            if (!current->second.recvBuffer.empty())
                            {
                                cerr
                                    << "Discarding incomplete "
                                    << "message at EOF, fd = "
                                    << fd
                                    << ", remaining bytes = "
                                    << current->second.recvBuffer.size()
                                    << endl;

                                current->second.recvBuffer.clear();
                            }
                        }

                        cout << "Client reached EOF, fd = "
                             << fd << endl;

                        // 尽可能发送剩余响应
                        if (!flushPending(fd))
                        {
                            shouldClose = true;
                        }
                        else
                        {
                            current = clients.find(fd);

                            if (current != clients.end() &&
                                current->second.pending.empty())
                            {
                                shouldClose = true;
                            }
                        }

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

            // 13. 处理连接挂断
            // 先前的读取逻辑已经有机会处理已到达的数据。
            // 如果仍有待发送数据，记录数量，再清理连接。
            if (flags & EPOLLHUP)
            {
                auto current = clients.find(fd);

                if (current != clients.end() &&
                    !current->second.pending.empty())
                {
                    cerr << "EPOLLHUP: closing fd = "
                         << fd
                         << ", unsent bytes = "
                         << current->second.pending.size()
                         << endl;
                }

                shouldClose = true;
            }

            // 14. 统一清理客户端
            if (shouldClose)
            {
                closeClient(fd);
            }
        }
    }

    // 15. 释放剩余资源
    for (const auto& item : clients)
    {
        close(item.first);
    }

    close(epfd);
    close(listenfd);

    return 0;
}
