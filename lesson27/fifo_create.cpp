#include <iostream>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

using namespace std;

int main()
{
    if(mkfifo("myfifo",0600) == -1)
    {
        cerr << "mkfifo failed: "<<strerror(errno)<<endl;//cout 用来输出正常信息，cerr 用来输出错误信息
        //把错误信息输出到标准错误流
        //Linux系统调用失败后，通过errno查看失败原因
        return 1;
    }
    cout<<"FIFO created"<<endl;

    return 0;
}
//mkfifo("myfifo", 0600) 创建一个名为myfifo的FIFO，请求仅允许文件所有者读写。
//如果返回-1，说明创建失败。
//errno保存对应的错误码，而strerror(errno)将错误码转换为可读的错误描述。