#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace std;

// ============================================================
// 第一部分：Channel
// 职责：保存 fd、关注的事件和事件回调
// ============================================================

class Channel {
private:
    int fd_;
    uint32_t events_;
    function<void(uint32_t)> callback_;

public:
    Channel(int fd, uint32_t events)
        : fd_(fd), events_(events) {}

    int fd() const {
        return fd_;
    }

    uint32_t events() const {
        return events_;
    }

    void setCallback(function<void(uint32_t)> callback) {
        callback_ = std::move(callback);
    }

    void handleEvent(uint32_t events) {
        if (callback_) {
            callback_(events);
        }
    }
};

// ============================================================
// 第二部分：Poller
// 职责：封装 epoll 的创建、注册、移除和等待
// ============================================================

class Poller {
private:
    int epollfd_;

public:
    Poller() : epollfd_(epoll_create1(EPOLL_CLOEXEC)) {
        if (epollfd_ == -1) {
            perror("epoll_create1");
            exit(EXIT_FAILURE);
        }
    }

    ~Poller() {
        if (epollfd_ != -1) {
            close(epollfd_);
        }
    }

    Poller(const Poller&) = delete;
    Poller& operator=(const Poller&) = delete;

    void addChannel(const Channel& channel) {
        epoll_event event{};
        event.events = channel.events();
        event.data.fd = channel.fd();

        if (epoll_ctl(
                epollfd_,
                EPOLL_CTL_ADD,
                channel.fd(),
                &event) == -1) {
            perror("epoll_ctl ADD");
            exit(EXIT_FAILURE);
        }
    }

    void removeChannel(int fd) {
        if (epoll_ctl(
                epollfd_,
                EPOLL_CTL_DEL,
                fd,
                nullptr) == -1) {
            if (errno != ENOENT && errno != EBADF) {
                perror("epoll_ctl DEL");
            }
        }
    }

    int waitEvents(epoll_event* events, int maxEvents) {
        int n = epoll_wait(
            epollfd_, events, maxEvents, -1);

        if (n == -1) {
            if (errno == EINTR) {
                return 0;
            }

            perror("epoll_wait");
            return -1;
        }

        return n;
    }
};

// ============================================================
// 第三部分：EventLoop
// 职责：等待事件，并将事件分发给对应的 Channel
// ============================================================

class EventLoop {
private:
    Poller poller_;
    unordered_map<int, Channel*> channels_;

public:
    void addChannel(Channel& channel) {
        if (channels_.count(channel.fd()) != 0) {
            cerr << "Channel already registered: "
                 << channel.fd() << endl;
            exit(EXIT_FAILURE);
        }

        poller_.addChannel(channel);
        channels_[channel.fd()] = &channel;
    }

    void removeChannel(int fd) {
        poller_.removeChannel(fd);
        channels_.erase(fd);
    }

    bool loopOnce() {
        epoll_event events[64]{};

        int n = poller_.waitEvents(events, 64);

        if (n == -1) {
            return false;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            auto it = channels_.find(fd);

            if (it != channels_.end()) {
                Channel* channel = it->second;
                channel->handleEvent(events[i].events);
            }
        }

        return true;
    }
};

// ============================================================
// 第四部分：辅助函数
// 职责：将套接字设置为非阻塞模式
// ============================================================

bool setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        perror("fcntl F_GETFL");
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        return false;
    }

    return true;
}

// ============================================================
// 第五部分：客户端读取函数
// 职责：读取数据，并记录需要清理的客户端
// 定义在 main() 外面
// ============================================================


void handleClientRead(
    int clientfd,
    uint32_t events,
    unordered_set<int>& pendingClose)
{
    // 第一步：检查是否有可读事件
    if (!(events & EPOLLIN)) {
        // 没有可读事件，但发生了错误或连接关闭事件
        if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            pendingClose.insert(clientfd);
        }

        return;
    }

    // 第二步：读取客户端数据
    char buffer[1024];
    ssize_t n;

    // 如果被信号中断，则重试读取
    do {
        n = read(clientfd, buffer, sizeof(buffer));
    } while (n == -1 && errno == EINTR);

    // 第三步：读取成功
    if (n > 0) {
        cout << "Received from client " << clientfd << ": ";
        cout.write(buffer, n);
        cout << '\n' << flush;
        return;
    }

    // 第四步：客户端正常关闭发送方向
    if (n == 0) {
        cout << "Client disconnected: " << clientfd << endl;
        pendingClose.insert(clientfd);
        return;
    }

    // 第五步：处理读取失败
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // 当前暂时没有数据可读
        // 如果同时存在关闭或错误事件，则安排清理
        if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            pendingClose.insert(clientfd);
        }

        return;
    }

    // 其他读取错误
    perror("read");
    pendingClose.insert(clientfd);
}


// ============================================================
// 第六部分：main
// 职责：初始化服务器、设置回调、运行事件循环
// ============================================================

int main() {
    // 1. 创建监听套接字
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenfd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    // 2. 允许地址复用
    int opt = 1;

    if (setsockopt(
            listenfd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)) == -1) {
        perror("setsockopt");
        close(listenfd);
        return EXIT_FAILURE;
    }

    // 3. 设置监听套接字为非阻塞模式
    if (!setNonBlocking(listenfd)) {
        close(listenfd);
        return EXIT_FAILURE;
    }

    // 4. 绑定地址和端口
    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(8080);

    if (bind(
            listenfd,
            reinterpret_cast<sockaddr*>(&serverAddr),
            sizeof(serverAddr)) == -1) {
        perror("bind");
        close(listenfd);
        return EXIT_FAILURE;
    }

    // 5. 开始监听
    if (listen(listenfd, SOMAXCONN) == -1) {
        perror("listen");
        close(listenfd);
        return EXIT_FAILURE;
    }

    // 6. 创建 Reactor 和客户端资源容器
    EventLoop loop;

    unordered_map<int, unique_ptr<Channel>> clientChannels;

    unordered_set<int> pendingClose;

    // 7. 创建监听 Channel
    Channel listenChannel(listenfd, EPOLLIN);

    // 8. 设置监听回调
    listenChannel.setCallback([&](uint32_t events) {
        if (!(events & EPOLLIN)) {
            return;
        }

        // 接受一个客户端连接
        int clientfd = accept(
            listenfd, nullptr, nullptr);

        if (clientfd == -1) {
            if (errno == EAGAIN ||
                errno == EWOULDBLOCK ||
                errno == EINTR) {
                return;
            }

            perror("accept");
            return;
        }

        // accepted socket 也需要设置为非阻塞模式
        if (!setNonBlocking(clientfd)) {
            close(clientfd);
            return;
        }

        cout << "New client fd: "
             << clientfd << endl;

        // 9. 创建客户端 Channel
        auto clientChannel =
            make_unique<Channel>(clientfd, EPOLLIN | EPOLLRDHUP);

        // 10. 设置客户端读取回调
        clientChannel->setCallback(
            [clientfd, &pendingClose](uint32_t clientEvents) {
                handleClientRead(
                    clientfd,
                    clientEvents,
                    pendingClose);
            });

        // 11. 注册客户端 Channel
        loop.addChannel(*clientChannel);

        // 12. 保存客户端 Channel 的所有权
        clientChannels[clientfd] = std::move(clientChannel);
    });

    // 13. 注册监听 Channel
    loop.addChannel(listenChannel);

    cout << "Reactor TCP server listening on port 8080"
         << endl;

    // 14. 运行事件循环
    while (loop.loopOnce()) {
        // 等本轮事件分发结束后，再清理客户端
        for (int fd : pendingClose) {
            loop.removeChannel(fd);

            auto it = clientChannels.find(fd);

            if (it != clientChannels.end()) {
                close(fd);
                clientChannels.erase(it);

                cout << "Cleaned up client fd: "
                     << fd << endl;
            }
        }

        pendingClose.clear();
    }

    // 15. 事件循环异常退出时，清理剩余客户端
    while (!clientChannels.empty()) {
        int fd = clientChannels.begin()->first;

        loop.removeChannel(fd);
        close(fd);
        clientChannels.erase(fd);
    }

    loop.removeChannel(listenfd);
    close(listenfd);

    return EXIT_FAILURE;
}



