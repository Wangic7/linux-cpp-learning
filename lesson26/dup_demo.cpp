
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

int main()
{
    // 1. 创建文件
    int fd = open("dup_text.txt",
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0644);

    if (fd == -1)
    {
        perror("open");
        return 1;
    }

    // 2. 复制文件描述符
    int newfd = dup(fd);

    if (newfd == -1)
    {
        perror("dup");
        close(fd);
        return 1;
    }

    // 3. 使用原始描述符写入
    ssize_t n = write(fd, "Hello ", 6);

    if (n != 6)
    {
        if (n == -1)
        {
            perror("write fd");
        }
        else
        {
            cerr << "第一次写入不完整" << endl;
        }

        close(newfd);
        close(fd);
        return 1;
    }

    // 4. 关闭原始描述符
    if (close(fd) == -1)
    {
        perror("close(fd)");
        close(newfd);
        return 1;
    }

    // 5. 使用复制后的描述符继续写入
    ssize_t newn = write(newfd, "Linux", 5);//newfd 可以继续写入，因为 dup() 创建的是另一个独立的文件描述符，它仍然引用相同的内核打开文件描述

    if (newn != 5)
    {
        if (newn == -1)
        {
            perror("write newfd");
        }
        else
        {
            cerr << "第二次写入不完整" << endl;
        }

        close(newfd);
        return 1;
    }

    // 6. 关闭复制后的描述符
    if (close(newfd) == -1)
    {
        perror("close(newfd)");
        return 1;
    }

    cout << "写入成功" << endl;

    return 0;
}
