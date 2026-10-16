#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
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

    int opt = 1;

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) == -1)
    {
        cerr << "setsockopt failed\n";
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

    cout << "server listening on port 8080\n";

    while (true)
    {
        cout << "waiting for client...\n";

        int connfd = accept(listenfd, nullptr, nullptr);

        if (connfd == -1)
        {
            cerr << "accept failed\n";
            continue;
        }

        cout << "client accepted, connfd = " << connfd << '\n';

        // 获取 connfd 当前的文件状态标志
        int flags = fcntl(connfd, F_GETFL, 0);

        if (flags == -1)
        {
            cerr << "fcntl F_GETFL failed\n";
            close(connfd);
            continue;
        }

        // 在原有 flags 基础上加入 O_NONBLOCK
        if (fcntl(connfd, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            cerr << "fcntl F_SETFL failed\n";
            close(connfd);
            continue;
        }

        cout << "connfd is now non-blocking\n";

        char buffer[1024]{};

        ssize_t n = read(connfd, buffer, sizeof(buffer) - 1);

        cout << "read returned: " << n << '\n';

        if (n == -1)
        {
            cout << "errno = " << errno << '\n';
        }

        if (n > 0)
        {
            cout << "client says: " << buffer << '\n';

            const char* response = "Hello from server\n";

            write(connfd, response, strlen(response));
        }

        close(connfd);

        cout << "connection closed\n";
    }

    close(listenfd);

    return 0;
}