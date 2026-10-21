
#include <iostream>
#include <functional>
#include <unordered_map>
#include <utility>
#include <cstdint>
#include <cerrno>
#include <cstdio>

#include <unistd.h>
#include <sys/epoll.h>

using namespace std;


// ==================== Channel ====================
// 保存文件描述符、关注的事件和事件处理回调
class Channel
{
public:
    Channel(int fd, uint32_t events)
        : fd_(fd), events_(events)
    {
    }

    int fd() const
    {
        return fd_;
    }

    uint32_t events() const
    {
        return events_;
    }

    void setCallback(function<void(uint32_t)> callback)
    {
        callback_ = move(callback);
    }

    void handleEvent(uint32_t revents)
    {
        if (callback_)
        {
            callback_(revents);
        }
    }

private:
    int fd_;
    uint32_t events_;
    function<void(uint32_t)> callback_;
};


// ==================== Poller ====================
// 负责 epoll 事件注册、移除和等待
class Poller
{
public:
    Poller()
        : epollFd_(epoll_create1(0))
    {
        if (epollFd_ == -1)
        {
            perror("epoll_create1");
        }
    }

    ~Poller()
    {
        if (epollFd_ != -1)
        {
            close(epollFd_);
        }
    }

    bool valid() const
    {
        return epollFd_ != -1;
    }

    bool addChannel(const Channel& channel)
    {
        if (epollFd_ == -1)
        {
            return false;
        }

        epoll_event event{};
        event.events = channel.events();
        event.data.fd = channel.fd();

        if (epoll_ctl(
                epollFd_,
                EPOLL_CTL_ADD,
                channel.fd(),
                &event) == -1)
        {
            perror("epoll_ctl ADD");
            return false;
        }

        return true;
    }

    bool removeChannel(int fd)
    {
        if (epollFd_ == -1)
        {
            return false;
        }

        if (epoll_ctl(
                epollFd_,
                EPOLL_CTL_DEL,
                fd,
                nullptr) == -1)
        {
            perror("epoll_ctl DEL");
            return false;
        }

        return true;
    }

    int waitEvents(epoll_event* events, int maxEvents)
    {
        if (epollFd_ == -1)
        {
            return -1;
        }

        return epoll_wait(
            epollFd_,
            events,
            maxEvents,
            1000
        );
    }

private:
    int epollFd_;
};


// ==================== EventLoop ====================
// 负责事件循环、查找 Channel 和分发事件
class EventLoop
{
public:
    bool addChannel(Channel& channel)
    {
        if (channels_.find(channel.fd()) != channels_.end())
        {
            cerr << "Channel already exists, fd = "
                 << channel.fd() << endl;
            return false;
        }

        if (!poller_.addChannel(channel))
        {
            return false;
        }

        channels_[channel.fd()] = &channel;

        cout << "Channel registered, fd = "
             << channel.fd() << endl;

        return true;
    }

    bool removeChannel(int fd)
    {
        auto it = channels_.find(fd);

        if (it == channels_.end())
        {
            cerr << "Channel not found, fd = "
                 << fd << endl;
            return false;
        }

        if (!poller_.removeChannel(fd))
        {
            return false;
        }

        channels_.erase(it);

        cout << "Channel removed, fd = "
             << fd << endl;

        return true;
    }

    void loopOnce()
    {
        epoll_event events[16];

        int n = poller_.waitEvents(events, 16);

        if (n == -1)
        {
            if (errno != EINTR)
            {
                perror("epoll_wait");
            }

            return;
        }

        for (int i = 0; i < n; ++i)
        {
            int fd = events[i].data.fd;

            auto it = channels_.find(fd);

            if (it == channels_.end())
            {
                continue;
            }

            it->second->handleEvent(events[i].events);
        }
    }

private:
    Poller poller_;

    // 保存文件描述符到 Channel 对象的映射
    unordered_map<int, Channel*> channels_;
};


// ==================== main ====================

int main()
{
    int pipeA[2] = {-1, -1};
    int pipeB[2] = {-1, -1};

    // 创建管道 A
    if (pipe(pipeA) == -1)
    {
        perror("pipe A");
        return 1;
    }

    // 创建管道 B
    if (pipe(pipeB) == -1)
    {
        perror("pipe B");
        close(pipeA[0]);
        close(pipeA[1]);
        return 1;
    }

    int exitCode = 0;

    {
        // 两个 Channel 分别管理两个管道的读端
        Channel channelA(pipeA[0], EPOLLIN);
        Channel channelB(pipeB[0], EPOLLIN);

        // 设置 Channel A 的读取回调
        channelA.setCallback(
            [fd = pipeA[0]](uint32_t flags)
            {
                if (flags & EPOLLIN)
                {
                    char buffer[128];

                    ssize_t n = read(
                        fd, buffer, sizeof(buffer)
                    );

                    if (n > 0)
                    {
                        cout << "Channel A received: ";
                        cout.write(buffer, n);
                        cout << endl;
                    }
                    else if (n == -1)
                    {
                        perror("read A");
                    }
                }
            }
        );

        // 设置 Channel B 的读取回调
        channelB.setCallback(
            [fd = pipeB[0]](uint32_t flags)
            {
                if (flags & EPOLLIN)
                {
                    char buffer[128];

                    ssize_t n = read(
                        fd, buffer, sizeof(buffer)
                    );

                    if (n > 0)
                    {
                        cout << "Channel B received: ";
                        cout.write(buffer, n);
                        cout << endl;
                    }
                    else if (n == -1)
                    {
                        perror("read B");
                    }
                }
            }
        );

        // 两个 Channel 共用同一个 EventLoop
        EventLoop loop;

        bool registeredA = false;
        bool registeredB = false;

        registeredA = loop.addChannel(channelA);

        if (!registeredA)
        {
            cerr << "Failed to register Channel A"
                 << endl;
            exitCode = 1;
        }

        if (exitCode == 0)
        {
            registeredB = loop.addChannel(channelB);

            if (!registeredB)
            {
                cerr << "Failed to register Channel B"
                     << endl;
                exitCode = 1;
            }
        }

        // 分别向两个管道写入数据
        if (exitCode == 0)
        {
            const char messageA[] = "Message for A";
            const char messageB[] = "Message for B";

            ssize_t nA = write(
                pipeA[1],
                messageA,
                sizeof(messageA) - 1
            );

            if (nA == -1)
            {
                perror("write A");
                exitCode = 1;
            }

            if (exitCode == 0)
            {
                ssize_t nB = write(
                    pipeB[1],
                    messageB,
                    sizeof(messageB) - 1
                );

                if (nB == -1)
                {
                    perror("write B");
                    exitCode = 1;
                }
            }
        }

        // 两个管道均已写入数据后，等待一轮事件
        if (exitCode == 0)
        {
            cout << "\nWaiting for events..."
                 << endl;

            loop.loopOnce();
        }

        // 移除已经成功注册的 Channel
        if (registeredA)
        {
            if (!loop.removeChannel(channelA.fd()))
            {
                cerr << "Failed to remove Channel A"
                     << endl;
                exitCode = 1;
            }
        }

        if (registeredB)
        {
            if (!loop.removeChannel(channelB.fd()))
            {
                cerr << "Failed to remove Channel B"
                     << endl;
                exitCode = 1;
            }
        }
    }

    // 关闭两个管道的读端和写端
    close(pipeA[0]);
    close(pipeA[1]);

    close(pipeB[0]);
    close(pipeB[1]);

    return exitCode;
}


