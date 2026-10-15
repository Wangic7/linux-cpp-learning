#include<unistd.h>
#include<fcntl.h>
#include<cstdio>

int main()
{
    int fd = open("server.log",O_WRONLY | O_CREAT | O_TRUNC,0644);

    if(fd == -1)
    {
        perror("open");
        return 1;
    }

   const char msg[] = "Server started\n";
   ssize_t n = write(fd,msg,sizeof(msg) - 1);

   if(n == -1)
   {
    perror("write");
    close(fd);
    return 1;
   }

   if(close(fd) == -1)
   {
    perror("close");
    return 1;
   }

   

   return 0;
}