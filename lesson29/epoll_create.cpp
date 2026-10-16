#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>

using namespace std;

int main()
{
    int epfd = epoll_create1(0);

    if (epfd == -1)
    {
        cerr << "epoll_create1 failed\n";
        return 1;
    }

    cout << "epoll fd = " << epfd << '\n';

    close(epfd);

    return 0;
}