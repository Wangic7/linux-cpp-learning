#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1)
    {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};

    // 设置IPv4
    addr.sin_family = AF_INET;
    // 设置端口8080
    addr.sin_port = htons(8080);
    // 设置IP地址127.0.0.1
    inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
    cout << "Socket address prepared" << endl;

    close(fd);

    return 0;
}