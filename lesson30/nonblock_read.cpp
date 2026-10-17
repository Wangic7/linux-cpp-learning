#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>

using namespace std;

int main()
{
    int flags = fcntl(STDIN_FILENO,F_GETFL,0);
    if(flags == -1)
    {
        cout<<"fcntl F_GETFL failed"<<endl;//也可用perror
        return 1;
    }
    if(fcntl(STDIN_FILENO,F_SETFL,flags || O_NONBLOCK) == -1)
    {
        cout<<"fcntl F_SETFL failed"<<endl;
        return 1;
    }

    char buf[1024];

    ssize_t n = read(STDIN_FILENO,buf,sizeof(buf));
    if(n == -1)
    {
        if(errno == EAGAIN | errno == EWOULDBLOCK)
        {
            cout<<"暂时没有数据"<<endl;
            return 0;
        }
        
        perror("read");
        return 1;
    }
    else if(n == 0)
    {
        cout<<"标准输入已结束(EOF)"<<endl;
    }
    else{
        cout<<"read bytes: ";
        cout.write(buf,n);//只输出实际读取的 n 个字节.如果用cout<<buf<<endl;则需要使用sizeof(buf) - 1，且要保证存在字符串结束符，即还需要手动添加字符串结束符 '\0'：buf[n] = '\0'
        cout<<endl;
    }
    
    return 0;
}