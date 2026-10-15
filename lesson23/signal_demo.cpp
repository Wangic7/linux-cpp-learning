#include <iostream>
#include <unistd.h>

using namespace std;

int main()
{
    // TODO 1：输出当前进程PID
    cout<<getpid()<<endl;
    cout << "Server started" << endl;

    while (true)
    {
        // TODO 2：输出 Server running...
        cout<<"Server running..."<<endl;
        // TODO 3：睡眠1秒
        sleep(3);
    }

    return 0;
}