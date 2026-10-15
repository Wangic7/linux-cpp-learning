#include<unistd.h>
#include<fcntl.h>
#include<cstdio>
#include<iostream>
#include <utility>
using namespace std;

class FdGuard
{
    private:
    int fd_;

    public:
     FdGuard(int fd) : fd_(fd)
    {
    }
    //构造函数：接受fd并初始化fd_，负责接受并保存文件描述符
   FdGuard(FdGuard&& other) noexcept
        : fd_(other.fd_)
    {
        other.fd_ = -1;
    }
    //析构函数 负责关闭资源
    ~FdGuard()
    {
        if(fd_ >= 0)
        {
            std::fprintf(stderr, "FdGuard destructor: closing fd=%d\n", fd_);
            close(fd_);
        }
    }


    FdGuard& operator = (FdGuard&& other) noexcept
    {
        if(this != &other)
        {
            if(fd_ >= 0)
            close(fd_);

            fd_ = other.fd_;
            other.fd = -1;
        }
        return *this;
    }

    //禁止复制
    FdGuard(const FdGuard&) = delete;
    FdGuard& operator = (const FdGuard&) = delete;

    //获取文件描述符
    int get() const//它没有任何形参，因此调用时也不能传入参数
    {
        return fd_;
    }
};//C++类定义结束时必须使用;  而不能只有} 

int main()
{

    int fd = open("raii_demo.txt",O_WRONLY | O_CREAT | O_TRUNC,0644);
    if(fd == -1)
    {
        perror("open");
        return 1;
    }

    FdGuard a(fd);

    cout << "Before move: a = " << a.get() << endl;

    FdGuard b(std::move(a));

    // 以下两行由你独立完成
    // 输出移动后a.get()的值
    cout<<"After move: a = "<<a.get()<<endl;
    // 输出移动后b.get()的值
    cout<<"After move: b = "<<b.get()<<endl;

    const char msg[] = "RAII works!\n";//定义字符数组
    ssize_t n = write(b.get(),msg,sizeof(msg) - 1);// 调用write()，保存返回值

    if(n == -1)//判断写入是否失败
    {
        perror("write");
        return 1;
    }

    return 0;
}