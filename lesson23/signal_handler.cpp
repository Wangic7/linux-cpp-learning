#include <iostream>
#include <unistd.h>
#include <csignal>
#include <cstdio>

using namespace std;

volatile sig_atomic_t stop_requested = 0;

// 信号处理函数
void handleSignal(int signum)
{
    // TODO 1:
    // 如果收到 SIGTERM
    // 将 stop_requested 设置为1
    if(signum == SIGTERM)
    {
        stop_requested = 1;
    }
}

int main()
{
    struct sigaction sa{};

    sa.sa_handler = handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGTERM, &sa, nullptr) == -1)
    {
        perror("sigaction failed");
        return 1;
    }

    // TODO 2:
    // 输出当前进程PID
    cout<<"PID: "<<getpid()<<endl;
    cout << "Server started" << endl;

    // TODO 3:
    // 当 stop_requested 为0时持续循环
    // 每次输出 Server running...
    // 然后 sleep(1)
    while(!stop_requested)
    {
        cout<<"Server running..."<<endl;
        sleep(1);
    }


    cout << "Server stopped gracefully" << endl;

    return 0;
}