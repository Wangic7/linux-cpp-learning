#include <arpa/inet.h>
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
        cerr << "socket failed\n";
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
        cerr << "bind failed\n";
        close(listenfd);
        return 1;
    }

    if (listen(listenfd, 10) == -1)
    {
        cerr << "listen failed\n";
        close(listenfd);
        return 1;
    }

    int epfd = epoll_create1(0);

    if (epfd == -1)
    {
        cerr << "epoll_create1 failed\n";
        close(listenfd);
        return 1;
    }

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = listenfd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &event) == -1)
    {
        cerr << "epoll_ctl failed\n";
        close(epfd);
        close(listenfd);
        return 1;
    }

    cout << "waiting for client...\n";

    epoll_event events[10];

    int n = epoll_wait(epfd, events, 10, -1);

    if (n == -1)
    {
        cerr << "epoll_wait failed\n";
        close(epfd);
        close(listenfd);
        return 1;
    }

    cout << "epoll_wait returned: " << n << '\n';

    for (int i = 0; i < n; ++i)
    {
        cout << "event fd = " << events[i].data.fd << '\n';

        if (events[i].data.fd == listenfd)
        {
            cout << "listenfd is ready, accepting client...\n";

            int connfd = accept(listenfd,nullptr,nullptr);

            if (connfd == -1)
            {
                cerr << "accept failed\n";
                continue;
            }

            cout << "client accepted, connfd = "
                 << connfd << '\n';


            epoll_event client_event{};
            client_event.events = EPOLLIN;
            client_event.data.fd = connfd;

            if(epoll_ctl(epfd,EPOLL_CTL_ADD,connfd,&client_event) == -1)
            {
                cerr<<"epoll_ctl connfd failed"<<endl;
                   close(connfd);
                   continue;
            }

            cout<<"connfd added to epoll"<<endl;

            // 第二次等待：等待客户端发送数据
            int m = epoll_wait(epfd, events, 10, -1);

            if (m == -1)
            {
                cerr << "second epoll_wait failed\n";
                close(connfd);
                continue;
            }

            for (int j = 0; j < m; ++j)
            {
                cout << "ready fd = "
                    << events[j].data.fd << '\n';

                cout << "event flags = "
                    << events[j].events << '\n';

                if (events[j].data.fd != listenfd)
                {
                    char buffer[1024]{};

                    ssize_t n = read(events[j].data.fd,buffer,sizeof(buffer) - 1);

                    if (n > 0)
                    {
                        cout << "client says: " << buffer <<endl;
                    }
                }
            }

            close(connfd);
        }
    }

    close(epfd);
    close(listenfd);

    return 0;
}
// 同一个 epoll_wait()，可以同时管理监听 socket 和大量客户端 socket

// 第一次 epoll_wait()
//         ↓
// listenfd = 3
//         ↓
// 新客户端连接
//         ↓
// accept()
//         ↓
// connfd = 5
//         ↓
// epoll_ctl(ADD, connfd)
//         ↓
// 第二次 epoll_wait()
//         ↓
// 客户端输入 hello
//         ↓
// connfd = 5 出现 EPOLLIN
//         ↓
// epoll_wait() 返回
