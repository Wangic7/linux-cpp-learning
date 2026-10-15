
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
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

    pid_t pid = fork();

    if (pid == -1)
    {
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    if (pid == 0)
    {
        // 子进程负责写入
        close(pipefd[0]);

        char message[] = "Hello Parent";

        // TODO 1：
        // 使用 write() 将message写入管道
        // 检查返回值是否为-1
       ssize_t n = write(pipefd[1], message, sizeof(message) - 1);//write()第三个参数是准备写入多少字节，不是用来存放返回值的
        if(n == -1)
        {
            perror("write failed");
            close(pipefd[1]);
            return 1;
        }

        close(pipefd[1]);
    }
    else
    {
        // 父进程负责读取
        close(pipefd[1]);

        char buffer[100];

        // TODO 2：
        // 使用 read() 从管道读取
        // 保存返回值到 ssize_t n
        ssize_t n = read(pipefd[0], buffer, sizeof(buffer));//read()的第三个参数表示本次最多请求读取多少字节
        // TODO 3：
        // 如果 n > 0
        // 使用 write() 输出到终端
        if(n > 0)
        {
            write(STDOUT_FILENO,buffer,n);
        }
        close(pipefd[0]);

        // TODO 4：
        // 使用 waitpid() 等待并回收子进程
        if(waitpid(pid,nullptr,0) == -1)
        {
            perror("waited failed");
            return 1;
        }
    }

    return 0;
}
