#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

int main()
{
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenfd == -1)
    {
        cerr << "socket failed" << endl;
        return 1;
    }

    int opt = 1;

    if (setsockopt(listenfd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) == -1)
    {
        cerr << "setsockopt failed" << endl;
        close(listenfd);
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenfd,
             reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) == -1)
    {
        cerr << "bind failed" << endl;
        close(listenfd);
        return 1;
    }

    if (listen(listenfd, 10) == -1)
    {
        cerr << "listen failed" << endl;
        close(listenfd);
        return 1;
    }

    int epfd = epoll_create1(0);

    if (epfd == -1)
    {
        cerr << "epoll_create1 failed" << endl;
        close(listenfd);
        return 1;
    }

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = listenfd;

    if (epoll_ctl(epfd,
                  EPOLL_CTL_ADD,
                  listenfd,
                  &event) == -1)
    {
        cerr << "epoll_ctl failed" << endl;
        close(epfd);
        close(listenfd);
        return 1;
    }

    cout << "waiting for client..." << endl;

    epoll_event events[10];

    while (true)
    {
        int n = epoll_wait(epfd, events, 10, -1);

        if (n == -1)
        {
            cerr << "epoll_wait failed" << endl;
            break;
        }

        cout << "epoll_wait returned: "
             << n << endl;

        for (int i = 0; i < n; ++i)
        {
            int fd = events[i].data.fd;

            cout << "ready fd = "
                 << fd << endl;

            // listenfd 有事件：有新的客户端连接
            if (fd == listenfd)
            {
                cout << "listenfd is ready" << endl;

                int connfd = accept(listenfd,
                                    nullptr,
                                    nullptr);

                if (connfd == -1)
                {
                    cerr << "accept failed" << endl;
                    continue;
                }

                cout << "client accepted, connfd = "
                     << connfd << endl;

                // 设置 connfd 为非阻塞模式
                int flags = fcntl(connfd, F_GETFL, 0);

                if (flags == -1)
                {
                    cerr << "fcntl F_GETFL failed" << endl;
                    close(connfd);
                    continue;
                }

                if (fcntl(connfd,
                          F_SETFL,
                          flags | O_NONBLOCK) == -1)
                {
                    cerr << "fcntl F_SETFL failed" << endl;
                    close(connfd);
                    continue;
                }

                cout << "connfd is now non-blocking"
                     << endl;

                // 把 connfd 加入 epoll
                epoll_event client_event{};

                client_event.events = EPOLLIN;
                client_event.data.fd = connfd;

                if (epoll_ctl(epfd,
                              EPOLL_CTL_ADD,
                              connfd,
                              &client_event) == -1)
                {
                    cerr << "epoll_ctl connfd failed"
                         << endl;

                    close(connfd);
                    continue;
                }

                cout << "connfd added to epoll"
                     << endl;
            }
            // 普通 connfd 有事件：客户端有数据
            else
            {
                char buffer[1024]{};

                ssize_t bytes_read =
                    read(fd,
                         buffer,
                         sizeof(buffer) - 1);

                if (bytes_read > 0)
                {
                    cout << "client says: "
                         << buffer << endl;

                    const char* response =
                        "Hello from epoll server\n";

                    write(fd,
                          response,
                          strlen(response));
                }
                else if (bytes_read == 0)
                {
                    cout << "client disconnected"
                         << endl;

                    close(fd);
                }
                else
                {
                    cerr << "read failed"
                         << endl;

                    close(fd);
                }
            }
        }
    }

    close(epfd);
    close(listenfd);

    return 0;
}