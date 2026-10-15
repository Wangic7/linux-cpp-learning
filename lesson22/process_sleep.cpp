#include <iostream>
#include <unistd.h>

using namespace std;

int main()
{
    // TODO 1：输出当前进程PID
    cout<<getpid()<<endl;
    // TODO 2：输出睡眠提示
    cout<<"Process is sleeping..."<<endl;
    // TODO 3：让程序睡眠30秒
    sleep(30);
    // TODO 4：输出结束提示
    cout<<"Process finished"<<endl;

    return 0;
}