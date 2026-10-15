
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
        // 子进程：只负责写
        close(pipefd[0]);

        char message[] = "Hello Pipe";

        if (write(pipefd[1], message, sizeof(message) - 1) == -1)
        {
            perror("write failed");
            close(pipefd[1]);
            return 1;
        }

        close(pipefd[1]);
        return 0;
    }
    else
    {
        // 父进程：只负责读
        close(pipefd[1]);

        char buffer[100];

        while (true)
        {
            // TODO 1:
            // 使用 read() 读取管道
            // 保存返回值到 ssize_t n
            ssize_t n = read(pipefd[0],buffer,sizeof(buffer));
            if (n > 0)
            {
                write(STDOUT_FILENO, buffer, n);
            }
            else if (n == 0)
            {
                // TODO 2:
                // 输出 "\nEOF reached"
                // 然后使用 break 退出循环
                cout<<"\nEOF reached"<<endl;
                break;
            }
            else
            {
                perror("read failed");
                break;
            }
        }

        close(pipefd[0]);

        if (waitpid(pid, nullptr, 0) == -1)
        {
            perror("waitpid failed");
            return 1;
        }
    }

    return 0;
}
