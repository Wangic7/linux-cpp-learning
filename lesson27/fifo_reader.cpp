#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

using namespace std;

int main()
{
    int fd = open("myfifo",O_RDONLY);

    if(fd == -1)
    {
        perror("open");
        return 1;
    }

    char buffer[128];

    while(true)
    {
         ssize_t n = read(fd,buffer,sizeof(buffer) - 1);
        if(n == -1)
        {
            perror("read");
            break;
        }
        if(n == 0)
        {
            cout<<"EOF"<<endl;
            break;
        }
        buffer[n] = '\0';
        cout<<buffer<<endl;
    }

    close(fd);
    return 0;
}