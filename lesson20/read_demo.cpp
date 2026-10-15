#include<unistd.h>
#include<fcntl.h>
#include<cstdio>

int main()
{
    int fd = open("server.log",O_RDONLY);
    // 1. 检查open是否失败
    if(fd == -1)
    {
        perror("open");
        return 1;
    }
    char buffer[100];

    // 2. 使用read读取数据，保存返回值
    ssize_t n = read(fd,buffer,sizeof(buffer));
    // 3. 检查read是否失败
    if(n == -1)
    {
        perror("read");
        close(fd);
        return 1;
    }
    // 4. 如果读取了数据，使用write输出
    if(n > 0)
    {
        write(STDOUT_FILENO,buffer,static_cast<size_t>(n));
    }
    
    // 5. 关闭文件并检查close结果
    if(close(fd) == -1)
    {
        perror("close");
        return 1;
    }

    return 0;
}