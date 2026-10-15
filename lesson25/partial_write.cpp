
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

using namespace std;

// 保证尽可能完整地写入指定长度的数据
bool writeAll(int fd, const char* data, size_t len)
{
    size_t total = 0;

    while (total < len)
    {
        ssize_t n = write(fd, data + total, len - total);

        if (n > 0)
        {
            total += static_cast<size_t>(n);
        }
        else if (n == 0)
        {
            return false;
        }
        else
        {
            perror("write");
            return false;
        }
    }

    return true;
}

int main()
{
    int fd = open("output.txt",
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0644);

    if (fd == -1)
    {
        perror("open");
        return 1;
    }

    const char* msg = "Hello Linux Server";

    bool ok = writeAll(fd, msg, strlen(msg));

    if (ok)
    {
        cout << "写入成功" << endl;
    }
    else
    {
        cout << "写入失败" << endl;
    }

    if (close(fd) == -1)
    {
        perror("close");
        return 1;
    }

    return ok ? 0 : 1;
}
