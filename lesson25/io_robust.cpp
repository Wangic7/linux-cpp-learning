#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

using namespace std;


bool writeALL(int fd,const char* data,size_t len)
{
    size_t total = 0;

    while(total < len)
    {
        ssize_t n = write(fd,data + total,len - total);
        if(n > 0)
        {
            total += static_cast<size_t>(n);
        }
        else if(n == 0){
            return false;
        }
        else{
            if(errno == EINTR)
            {
                continue;
            }
            perror("write");
            return false;
        }
    }
    return true;
}

bool readALL(int fd,char* buffer,size_t len)
{
    size_t total = 0;
    while(total < len)
    {
        ssize_t n = read(fd,buffer + total,len - total);
        if(n > 0)
        {
            total += static_cast<size_t>(n);
        }
        else if(n == 0)
        {
            return false;
        }
        else{
            if(errno == EINTR)
            {
                continue;
            }
            perror("read failed");
            return false;
        }
    }
    return true;
}
int main()
{
    const char* msg = "Linux IO Robustness";
    size_t len = strlen(msg);


    int fd = open("message.txt",O_WRONLY | O_CREAT |O_TRUNC,0644);
    if(fd == -1)
    {
        perror("open");
        return 1;
    }

    bool ok = writeALL(fd,msg,len);
    if(!ok)
    {
        close(fd);
        return 1;
    }

    if(close(fd) == -1)
    {
        perror("close");
        return 1;
    }

    fd = open("message.txt",O_RDONLY);
    char buffer[100] = {};

     if(fd == -1)
    {
        perror("open");
        return 1;
    }

    bool readok = readALL(fd,buffer,len);
    if(readok)
    {
        cout<<buffer<<endl;
    }
    else{
        cout<<"读取失败"<<endl;
    }

     if(close(fd) == -1)
    {
        perror("close");
        return 1;
    }

    return readok ? 0 : 1;
}