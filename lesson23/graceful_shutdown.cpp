
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <csignal>
#include <cstdio>

using namespace std;

// 全局停止标志
volatile sig_atomic_t stop_requested = 0;

// 信号处理函数
void handleSignal(int signum)
{
    if (signum == SIGTERM)
    {
        stop_requested = 1;
    }
}

int main()
{
    // 1. 注册 SIGTERM 信号处理函数
    struct sigaction sa{};

    sa.sa_handler = handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGTERM, &sa, nullptr) == -1)
    {
        perror("sigaction failed");
        return 1;
    }

    // 2. 打开日志文件
    ofstream log("server.log", ios::app);

    if (!log.is_open())
    {
        cerr << "Log file open failed" << endl;
        return 1;
    }

    // 3. 服务器启动
    cout << "PID: " << getpid() << endl;
    cout << "Server started" << endl;

    log << "Server started" << endl;

    if (!log)
    {
        cerr << "Log write failed" << endl;
        return 1;
    }

    // 4. 服务器主循环
    while (!stop_requested)
    {
        cout << "Server running..." << endl;
        sleep(1);
    }

    // 5. 收到 SIGTERM 后正常退出循环
    cout << "Server stopped gracefully" << endl;

    log << "Server stopped gracefully" << endl;

    if (!log)
    {
        cerr << "Log write failed" << endl;
        return 1;
    }

    // 6. 正常返回，ofstream析构并关闭日志文件
    return 0;
}
