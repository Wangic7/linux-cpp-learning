#include <iostream>
#include <unistd.h>
#include <cstdio>

using namespace std;

int main()
{
    int pipefd[2];

    if (pipe(pipefd) == -1)
    {
        perror("pipe failed");
        return 1;
    }

    char message[] = "Hello IPC";

    // TODO 1：
    // 使用 write() 向管道写入9个字节
    ssize_t n = write(pipefd[1],message,sizeof(message) - 1);
    if(n == -1)
    {
        perror("write failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    char buffer[100];

    // TODO 2：
    // 使用 read() 从管道读取数据
    // 把返回值保存到 ssize_t n
    n =read(pipefd[0],buffer,sizeof(buffer));
    if (n > 0)
    {
        // TODO 3：
        // 使用 write() 将读取的数据输出到终端
       write(STDOUT_FILENO, buffer, n);
    }
    else if(n == -1)
    {
        perror("read failed");
    }

    // TODO 4：
    // 关闭管道的读取端和写入端
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}