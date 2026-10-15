#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>

using namespace std;

int main()
{
    pid_t pid = fork();

    if (pid == -1)
    {
        cout << "fork failed" << endl;
        return 1;
    }

    if (pid == 0)
    {
        cout << "Child started" << endl;

        // TODO 1：调用 exit(0) 结束子进程
        exit(0);
        cout << "Child should not print this" << endl;
    }
    else
    {
        // TODO 2：使用 waitpid 等待子进程
        // 并判断是否失败
        if(waitpid(pid,nullptr,0) == -1)
        {
            cout<<"waitpid failed"<<endl;
            return 1;
        }
        else{
            cout<<"Parent start"<<endl;
        }
        cout << "Parent finished" << endl;
    }

    return 0;
}