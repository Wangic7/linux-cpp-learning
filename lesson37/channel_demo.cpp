#include <iostream>
#include <functional>
#include <utility>
#include <sys/epoll.h>

using namespace std;

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

    void setReadCallback(function<void()> callback)
    {
        readCallback_ = move(callback);
    }

    void setWriteCallback(function<void()> callback)
    {
        writeCallback_ = move(callback);
    }

    void handleEvent(uint32_t revents)
    {
        if ((revents & EPOLLIN) && readCallback_)
        {
            readCallback_();
        }

        if ((revents & EPOLLOUT) && writeCallback_)
        {
            writeCallback_();
        }
    }

private:
    int fd_;
    uint32_t events_;

    function<void()> readCallback_;
    function<void()> writeCallback_;
};

int main()
{
    int clientFd = 5;

    Channel channel(clientFd, EPOLLIN | EPOLLOUT);

    channel.setReadCallback([clientFd]()
    {
        cout << "客户端 " << clientFd
             << " 触发读取处理" << endl;
    });

    channel.setWriteCallback([clientFd]()
    {
        cout << "客户端 " << clientFd
             << " 触发写入处理" << endl;
    });

    cout << "监控的 fd: " << channel.fd() << endl;

    cout << "\n模拟可读事件：" << endl;
    channel.handleEvent(EPOLLIN);

    cout << "\n模拟可写事件：" << endl;
    channel.handleEvent(EPOLLOUT);

    cout << "\n模拟同时可读和可写：" << endl;
    channel.handleEvent(EPOLLIN | EPOLLOUT);

    return 0;
}