#include<unistd.h>
#include<fcntl.h>
#include<cstdio>
#include<iostream>
using namespace std;


int main()
{
    int fd1 = open("a.txt",O_WRONLY | O_CREAT | O_TRUNC,0644);
    if(fd1 == -1)
    {
        perror("open");
        return 1;
    }
    cout<<"fd1 = "<<fd1<<endl;

    int fd2 = open("b.txt",O_WRONLY | O_CREAT | O_TRUNC,0644);
    if(fd2 == -1)
    {
        perror("open");
        return 1;
    }
    cout<<"fd2 = "<<fd2<<endl;

    if(close(fd1) == -1)
    {
        perror("close");
        return 1;
    }

    int fd3 = open("c.txt",O_WRONLY | O_CREAT | O_TRUNC,0644);
    if(fd3 == -1)
    {
        perror("open");
        return 1;
    }
    cout<<"fd3 = "<<fd3<<endl;

    if(close(fd2) == -1)
    {
        perror("close");
        return 1;
    }

    if(close(fd3) == -1)
    {
        perror("close");
        return 1;
    }



    return 0;
}