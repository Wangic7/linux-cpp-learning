
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstdio>
#include <cstring>
#include <string>

using namespace std;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        cout << "用法: /tmp/lt_et_demo lt|et" << endl;
        return 1;
    }

    string mode = argv[1];

    if (mode != "lt" && mode != "et")
    {
        cout << "参数只能是 lt 或 et" << endl;
        return 1;
    }

    // 1. 创建匿名管道
    int pipefd[2];

    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return 1;
    }

    // 2. 将管道读端设置为非阻塞
    int flags = fcntl(pipefd[0], F_GETFL, 0);

    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    if (fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    // 3. 创建 epoll 实例
    int epfd = epoll_create1(0);

    if (epfd == -1)
    {
        perror("epoll_create1");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    // 4. 注册管道读端
    epoll_event ev{};
    ev.events = EPOLLIN;

    if (mode == "et")
    {
        ev.events |= EPOLLET;
    }

    ev.data.fd = pipefd[0];

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &ev) == -1)
    {
        perror("epoll_ctl");
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    // 5. 向管道写入 100 字节
    char data[100];
    memset(data, 'A', sizeof(data));

    ssize_t written = write(pipefd[1], data, sizeof(data));

    if (written == -1)
    {
        perror("write");
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    cout << "模式: " << (mode == "lt" ? "LT" : "ET") << endl;
    cout << "写入字节数: " << written << endl;

    // 6. 第一次等待可读事件
    epoll_event ready{};

    int count = epoll_wait(epfd, &ready, 1, 1000);

    if (count == -1)
    {
        perror("epoll_wait");
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    if (count == 0)
    {
        cout << "第一次等待超时" << endl;
        close(epfd);
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    cout << "第一次 epoll_wait: 收到可读事件" << endl;

    
    // 7. 循环读取，直到 EAGAIN
    char buf[20];
    size_t total_read = 0;

    while (true)
    {
        ssize_t n = read(pipefd[0], buf, sizeof(buf));

        if (n > 0)
        {
            cout << "本次读取: " << n << " 字节" << endl;
            total_read += n;
        }
        else if (n == 0)
        {
            cout << "读取结束（EOF）" << endl;
            break;
        }
        else
        {
            if (errno == EAGAIN)
            {
                cout << "read 返回 EAGAIN，当前没有更多数据" << endl;
                break;
            }

            if (errno == EINTR)
            {
                continue;
            }

            perror("read");
            close(epfd);
            close(pipefd[0]);
            close(pipefd[1]);
            return 1;
        }
    }

    cout << "累计读取字节数: " << total_read << endl;

    // 8. 再次等待，观察 LT 和 ET 的区别
    count = epoll_wait(epfd, &ready, 1, 500);

    if (count == -1)
    {
        perror("epoll_wait");
    }
    else if (count == 0)
    {
        cout << "第二次 epoll_wait: 等待超时，没有收到新事件" << endl;
    }
    else
    {
        cout << "第二次 epoll_wait: 再次收到可读事件" << endl;
    }

    close(epfd);
    close(pipefd[0]);
    close(pipefd[1]);

    return 0;
}
