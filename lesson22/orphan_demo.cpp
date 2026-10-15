#include <iostream>
#include <unistd.h>
#include <cstdlib>

using namespace std;

int main()
{
    pid_t original_parent = getpid();

    pid_t pid = fork();

    if (pid == -1)
    {
        cout << "fork failed" << endl;
        return 1;
    }

    if (pid == 0)
    {
        // TODO 1：sleep(3)，等待3秒
        sleep(3);
        // TODO 2：输出 original_parent
        cout<< original_parent <<endl;
        // TODO 3：使用 getppid() 输出当前父进程PID
        cout<<getppid()<<endl;
    }
    else
    {
        cout << "Parent exiting" << endl;

        // TODO 4：使用 _exit(0) 结束父进程
        _exit(0);
    }

    return 0;
}