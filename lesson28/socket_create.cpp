#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

int main()
{
    int fd = socket(AF_INET,SOCK_STREAM,0);
    if (fd == -1)
    {
        perror("socket");
        return 1;
    }

    cout << "Socket created, fd = " << fd << endl;

    close(fd);

    return 0;
}