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

    // ===== 从这里开始是今天的新知识 =====

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

    if (epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&event) == -1)
    {
        cerr << "epoll_ctl failed\n";
        close(epfd);
        close(listenfd);
        return 1;
    }

    cout << "listenfd added to epoll\n";

    close(epfd);
    close(listenfd);

    return 0;
}