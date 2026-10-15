#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdio>

using namespace std;

int main()
{
    int pipefd[2];

    // 1. 创建管道并检查错误
    if(pipe(pipefd) == -1)
    {
        perror("pipe failed");
        return 1;
    }
    // 2. fork创建子进程并检查错误
    pid_t pid = fork();
    if (pid == 0)
    {
        // 3. 关闭读取端
        close(pipefd[0]);
        // 4. 发送msg1和msg2
        char msg1[] = "Task A completed\n";
        if(write(pipefd[1],msg1,sizeof(msg1) - 1) == -1)
        {
            perror("write msg1 failed");
            close(pipefd[1]);
            return 1;
        }
        char msg2[] = "Task B completed\n";
        if(write((pipefd[1]),msg2,sizeof(msg2) - 1) == -1)
        {
            perror("write msg2 failed");
            close(pipefd[1]);
            return 1;
        }
        // 5. 关闭写入端
        close(pipefd[1]);
    }
    else if(pid == -1)
    {
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }
    else
    {
        // 6. 关闭写入端
        close(pipefd[1]);
        char buffer[100];
        // 7. read循环读取到EOF
        while(true)
        {
            ssize_t n = read(pipefd[0],buffer,sizeof(buffer));
            if(n > 0)
            {
                write(STDOUT_FILENO,buffer,n);//write(目标fd, 数据地址, 写入字节数);
            }
            else if(n == 0)
            {
                cout<<"All tasks received"<<endl;
                break;
            }
            else{
                perror("read failed");
               break;
            }
        }
        // 8. 关闭读取端
        close(pipefd[0]);
        // 9. waitpid回收子进程
        if(waitpid(pid,nullptr,0) == -1)
        {
            perror("waitpid failed");
            return 1;
        }
    }

    return 0;
}