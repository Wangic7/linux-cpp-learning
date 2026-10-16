#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

using namespace std;

bool writeAll(int fd, const char* data, size_t length)
{
    size_t total = 0;
    while(total < length)
    {
        ssize_t n = write(fd,data + total,length - total);
        if(n == -1)
        {
            if(errno == EINTR)
            {
                continue;
            }

            return false;
        }

        total += n;
    }

    return true;
}



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
    
    if(connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr)) == -1)
    {
        perror("connect");
        close(fd);
        return 1;
    }

    cout<<"Connected to server"<<endl;

    const char* message1 = "Hello";
    if (!writeAll(fd, message1, strlen(message1)))
    {
    perror("writeAll");
    close(fd);
    return 1;
    }

    const char* message2 = "World";
    if (!writeAll(fd, message2, strlen(message2)))
    {
    perror("writeAll");
    close(fd);
    return 1;
    }

    char buffer[128];
    ssize_t n = read(fd,buffer,sizeof(buffer) - 1);
    if(n == -1)
    {
        perror("read");
        close(fd);
        return 1;
    }

    buffer[n] = '\0';

    cout<<"Received: "<< buffer << endl;

    close(fd); 

    return 0;
}