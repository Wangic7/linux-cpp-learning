#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

int main()
{
    int fd = socket(AF_INET,SOCK_STREAM,0);
    if(fd == -1)
    {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);

    if(bind(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr)) == -1 )
    {
        perror("bind");
        close(fd);
        return 1;
    }
    cout<<"Socket bind successful"<<endl;

    close(fd);
    return 0;
}