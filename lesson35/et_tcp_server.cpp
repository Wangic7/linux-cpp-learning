
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

// 每个客户端自己的状态
struct ClientState
{
    string pending;          // 尚未发送的数据
    bool peerWriteClosed = false; // 对端是否已经停止发送
    string recvBuffer;  // 新增：保存尚未组成完整消息的数据
};

// 设置非阻塞模式
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

    // 3. 创建 epoll
    int epfd = epoll_create1(0);

    if (epfd == -1)
    {
        perror("epoll_create1");
        close(listenfd);
        return 1;
    }

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

    cout << "Lesson 33 TCP server listening on port 8080"
         << endl;

    // 保存所有客户端的状态
    unordered_map<int, ClientState> clients;

    epoll_event events[16]{};

    // 4. 关闭客户端并释放状态
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

    // 5. 尝试发送指定客户端的待发送数据
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
                (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                static bool printedEagain = false;

                if (!printedEagain)
                {
                    cout << "write reached EAGAIN; pending = "
                         << state.pending.size()
                         << " bytes" << endl;
                    printedEagain = true;
                }

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

        // 对端已经停止发送，且响应也发送完毕。
        // 由调用者负责关闭连接。
        if (state.peerWriteClosed && state.pending.empty())
        {
            return true;
        }

        // 对端仍能发送时，关注可读和半关闭事件。
        // 只有还有待发送数据时，才关注 EPOLLOUT。
        epoll_event clientEvent{};
        clientEvent.events = EPOLLET;

        if (!state.peerWriteClosed)
        {
            clientEvent.events |= EPOLLIN | EPOLLRDHUP;
        }

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

    // 6. 主事件循环
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
            uint32_t flags = events[i].events;

            // 监听 Socket：接受新连接
            if (fd == listenfd)
            {
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

                    if (!setNonBlocking(connfd))
                    {
                        close(connfd);
                        continue;
                    }

                    epoll_event clientEvent{};
                    clientEvent.events =
                        EPOLLIN | EPOLLRDHUP | EPOLLET;
                    clientEvent.data.fd = connfd;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD,
                                  connfd, &clientEvent) == -1)
                    {
                        perror("epoll_ctl ADD");
                        close(connfd);
                        continue;
                    }

                    clients.emplace(connfd, ClientState{});

                    cout << "Client connected, fd = "
                         << connfd << endl;
                }

                continue;
            }

            // 已有客户端
            auto it = clients.find(fd);

            // 忽略已经清理的客户端
            if (it == clients.end())
            {
                continue;
            }

            bool shouldClose = false;

            // 发生 Socket 错误
            if (flags & EPOLLERR)
            {
                int socketError = 0;
                socklen_t errorLen = sizeof(socketError);

                if (getsockopt(fd, SOL_SOCKET, SO_ERROR,
                               &socketError, &errorLen) == -1)
                {
                    perror("getsockopt SO_ERROR");
                }
                else
                {
                    cerr << "EPOLLERR: fd = " << fd
                         << ", SO_ERROR = "
                         << socketError << endl;
                }

                shouldClose = true;
            }

            // Socket 再次可写，继续发送剩余数据
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

            // 可读、对端半关闭或连接挂断时，尝试读取
            if (!shouldClose &&
                (flags & (EPOLLIN | EPOLLRDHUP | EPOLLHUP)))
            {
                while (true)
                {
                    char buffer[1024];

                    ssize_t n = read(fd, buffer, sizeof(buffer));

                    if (n > 0)
                    {
                        auto current = clients.find(fd);

                        if (current == clients.end())
                        {
                            shouldClose = true;
                            break;
                        }

                        // 第一步：保存刚刚收到的数据
                        current->second.recvBuffer.append(
                            buffer, static_cast<size_t>(n)
                        );

                        // 第二步：检查缓冲区中是否存在完整消息
                        while (true)
                        {
                            size_t pos = current->second.recvBuffer.find('\n');

                            // 没有换行符，剩余数据暂时不完整
                            if (pos == string::npos)
                            {
                                break;
                            }

                            // 提取一条完整消息，不包含换行符
                            string message = current->second.recvBuffer.substr(0, pos);

                            cout << "Complete message from fd " << fd
                                << ": [" << message << "]" << endl;

                            // 将完整消息放入待发送缓冲区
                            current->second.pending.append(message);
                            current->second.pending.push_back('\n');

                            // 删除已处理的消息，保留剩余数据
                            current->second.recvBuffer.erase(0, pos + 1);
                        }

                        cout << "Received " << n
                             << " bytes from fd "
                             << fd << endl;

                        if (!flushPending(fd))
                        {
                            shouldClose = true;
                            break;
                        }

                        continue;
                    }

                    if (n == 0)
                    {
                        auto current = clients.find(fd);

                        if (current != clients.end())
                        {
                            current->second.peerWriteClosed = true;

                            // EOF 时检查是否还有不完整消息
                            if (!current->second.recvBuffer.empty())
                            {
                                cout << "Discarding incomplete message at EOF, fd = "
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

            // HUP 表示连接挂断。本课先按断开处理。
            if (flags & EPOLLHUP)
            {
                shouldClose = true;
            }

            if (shouldClose)
            {
                closeClient(fd);
            }
        }
    }

    // 7. 释放资源
    for (const auto& item : clients)
    {
        close(item.first);
    }

    close(epfd);
    close(listenfd);

    return 0;
}
