#include <iostream>
#include <unordered_set>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

using namespace std;

// 设置文件描述符为非阻塞模式
bool setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
    {
        return false;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

class Reactor
{
public:
    bool init(int port)
    {
        // 1. 创建监听 Socket
        listenFd_ = socket(AF_INET, SOCK_STREAM, 0);

        if (listenFd_ == -1)
        {
            perror("socket");
            return false;
        }

        int reuse = 1;

        if (setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR,
                       &reuse, sizeof(reuse)) == -1)
        {
            perror("setsockopt");
            return false;
        }

        if (!setNonBlocking(listenFd_))
        {
            perror("fcntl");
            return false;
        }

        // 2. 绑定地址
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(listenFd_,
                 reinterpret_cast<sockaddr*>(&addr),
                 sizeof(addr)) == -1)
        {
            perror("bind");
            return false;
        }

        // 3. 开始监听
        if (listen(listenFd_, SOMAXCONN) == -1)
        {
            perror("listen");
            return false;
        }

        // 4. 创建 epoll
        epollFd_ = epoll_create1(0);

        if (epollFd_ == -1)
        {
            perror("epoll_create1");
            return false;
        }

        // 5. 将监听 Socket 注册到 epoll
        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = listenFd_;

        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD,
                      listenFd_, &event) == -1)
        {
            perror("epoll_ctl");
            return false;
        }

        return true;
    }

    void run()
    {
        epoll_event events[64];

        while (true)
        {
            int n = epoll_wait(
                epollFd_, events, 64, -1
            );

            if (n == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                perror("epoll_wait");
                return;
            }

            // 事件循环：遍历本轮返回的事件
            for (int i = 0; i < n; ++i)
            {
                dispatchEvent(events[i]);
            }
        }
    }

    ~Reactor()
    {
        for (int fd : clients_)
        {
            close(fd);
        }

        if (epollFd_ != -1)
        {
            close(epollFd_);
        }

        if (listenFd_ != -1)
        {
            close(listenFd_);
        }
    }

private:
    void dispatchEvent(const epoll_event& event)
    {
        int fd = event.data.fd;
        uint32_t flags = event.events;

        // 监听 Socket：处理新连接
        if (fd == listenFd_)
        {
            if (flags & EPOLLIN)
            {
                handleAccept();
            }

            return;
        }

        // 客户端 Socket：优先处理可读事件
        if (flags & EPOLLIN)
        {
            handleRead(fd);
            return;
        }

        // 本示例中的简化异常处理
        if (flags & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
        {
            closeClient(fd);
        }
    }

    void handleAccept()
    {
        int clientFd = accept(listenFd_, nullptr, nullptr);

        if (clientFd == -1)
        {
            if (errno != EINTR &&
                errno != EAGAIN &&
                errno != EWOULDBLOCK)
            {
                perror("accept");
            }

            return;
        }

        if (!setNonBlocking(clientFd))
        {
            perror("fcntl client");
            close(clientFd);
            return;
        }

        epoll_event event{};
        event.events = EPOLLIN | EPOLLRDHUP;
        event.data.fd = clientFd;

        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD,
                      clientFd, &event) == -1)
        {
            perror("epoll_ctl client");
            close(clientFd);
            return;
        }

        clients_.insert(clientFd);

        cout << "Client connected, fd = "
             << clientFd << endl;
    }

    void handleRead(int fd)
    {
        char buffer[4096];

        ssize_t n = read(fd, buffer, sizeof(buffer));

        if (n > 0)
        {
            cout << "[fd " << fd << "] received "
                 << n << " bytes: ";

            cout.write(buffer, n);
            cout << endl;
            return;
        }

        if (n == 0)
        {
            closeClient(fd);
            return;
        }

        if (errno == EINTR ||
            errno == EAGAIN ||
            errno == EWOULDBLOCK)
        {
            return;
        }

        perror("read");
        closeClient(fd);
    }

    void closeClient(int fd)
    {
        if (clients_.erase(fd) == 0)
        {
            return;
        }

        epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);

        cout << "Client closed, fd = "
             << fd << endl;
    }

private:
    int listenFd_ = -1;
    int epollFd_ = -1;

    unordered_set<int> clients_;
};

int main()
{
    Reactor server;

    if (!server.init(8080))
    {
        return 1;
    }

    cout << "Reactor server listening on port 8080"
         << endl;

    server.run();

    return 0;
}
