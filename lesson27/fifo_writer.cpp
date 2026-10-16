#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

using namespace std;

int main()
{
    int fd = open("myfifo", O_WRONLY);

    // 判断open是否失败
    if(fd == -1)
    {
        perror("open");
        return 1;
    }

    const char* message = "Hello FIFO";

    // 调用write()
    ssize_t n = write(fd,message,strlen(message));

    // 判断write是否失败
    if(n == -1)
    {
        perror("write");
        return 1;
    }

    close(fd);

    return 0;
}