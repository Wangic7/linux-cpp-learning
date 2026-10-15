#include <unistd.h>
#include <fcntl.h>
#include <cstdio>

int main()
{
    int fd = open("server.log", O_RDONLY);

    // 1. 检查open是否失败
    if(fd == -1)
    {
        perror("open");
        return 1;
    }
    char buffer[100]{};

    // 2. 第一次读取5个字节
    // ssize_t n1 = ...
    ssize_t n1 = read(fd,buffer,5);
    // 3. 如果读取失败，报错并关闭fd
    // 如果读取成功，输出实际读取的数据
    if(n1 == -1)
    {
        perror("read");
        close(fd);
        return 1;
    }
    if (n1 > 0)
    {
    write(STDOUT_FILENO, buffer, static_cast<size_t>(n1));
    }
    
    // 4. 使用lseek将偏移量恢复到0
    off_t pos = lseek(fd, 0, SEEK_SET);

    // 5. 检查lseek是否失败
    if(pos == -1)
    {
        perror("lseek");
        close(fd);
        return 1;
    }
    // 6. 第二次读取5个字节
    // ssize_t n2 = ...
    ssize_t n2 = read(fd,buffer,5);
    // 7. 输出第二次读取的数据
    if(n2 == -1)
    {
        perror("read");
        close(fd);
        return 1;
    }
    if(n2 > 0)
    {
        write(STDOUT_FILENO,buffer,static_cast<size_t>(n2));
    }
    // 8. 关闭fd，检查关闭是否失败
    if(close(fd) == -1)
    {
        perror("close");
        return 1;
    }
    return 0;
}