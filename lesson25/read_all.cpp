#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

using namespace std;

bool readAll(int fd, char* buffer, size_t len)
{
    size_t total = 0;

    // TODO: 独立完成完整读取循环
    while(total < len)
    {
        ssize_t n = read(fd,buffer + total,len - total);
        if(n > 0)
        {
            total += static_cast<size_t>(n);
        }
        else if(n == 0)
        {
            return false;
        }
        else{
            if(errno == EINTR)
            {
                continue;
            }
            perror("read");
            return false;
        }
    }
    return true;
}

int main()
{
    int fd = open("output.txt", O_RDONLY);

    if (fd == -1)
    {
        perror("open");
        return 1;
    }

    char buffer[19] = {};

    bool ok = readAll(fd, buffer, 19);

    if (ok)
    {
        cout << "读取成功：" << buffer << endl;
    }
    else
    {
        cout << "读取失败" << endl;
    }

    if (close(fd) == -1)
    {
        perror("close");
        return 1;
    }

    return ok ? 0 : 1;
}