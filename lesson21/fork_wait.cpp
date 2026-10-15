#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

int main()
{
    pid_t pid = fork();
    if(pid == -1)
    {
        cout<<"fork failed"<<endl;
        return 1;
    }
    if(pid == 0)
    {
        cout<<"Child PID: "<<getpid()<<endl;
        cout<<"Child finished"<<endl;
    }
    else
    {
        if(waitpid(pid,nullptr,0) == -1)
        {
            cout<<"waitpid failed"<<endl;
            return 1;
        }

        cout<<"Parent PID: "<<getpid()<<endl;
        cout<<"Parent finished"<<endl;
    }

    return 0;
}