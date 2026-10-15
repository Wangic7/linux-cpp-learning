#include <iostream>
#include <unistd.h>
#include <fcntl.h>

using namespace std;



int main()
{
    int fd = open("redirect.txt",O_WRONLY | O_CREAT |O_TRUNC,0644);
    if(fd == -1)
    {
        perror("open");
        return 1;
    }

    if(dup2(fd,STDOUT_FILENO) == -1)
    {
        perror("dup2");
        close(fd);
        return 1;
    }

    if(close(fd) == -1)
    {
        perror("close");
        return 1;
    }

    cout<<"Linux Server"<<endl;
    cout<<"Lesson 26 dup2"<<endl;

    return 0;
}