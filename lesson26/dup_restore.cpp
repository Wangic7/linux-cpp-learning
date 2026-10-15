#include <iostream>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

int main()
{
    // TODO 1：使用 dup() 保存标准输出
    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout == -1)
    {
    perror("dup saved_stdout");
    return 1;
    }
    // TODO 2：使用 open() 创建 restore.txt
    int fd = open("restore.txt",O_WRONLY | O_CREAT |O_TRUNC,0644);
    if(fd == -1)
    {
        perror("open");
        return 1;
    }
    // TODO 3：使用 dup2() 重定向标准输出
    if(dup2(fd,STDOUT_FILENO) == -1)
    {
        perror("dup2");
        close(fd);
        close(saved_stdout);
        return 1;
    }
    // TODO 4：关闭原始文件描述符 fd
    if(close(fd) == -1)
    {
        perror("close");
        return 1;
    }

    cout << "Hello File" << endl;

    // TODO 5：使用 dup2() 恢复标准输出
    // TODO 6：关闭 saved_stdout
    if (dup2(saved_stdout, STDOUT_FILENO) == -1)
    {
    perror("restore stdout");
    close(saved_stdout);
    return 1;
    }

    if (close(saved_stdout) == -1)
    {
    perror("close saved_stdout");
    return 1;
    }

    cout << "Hello Terminal" << endl;

    return 0;
}

// int saved = dup(STDOUT_FILENO);  // 保存终端

// dup2(fd, STDOUT_FILENO);         // stdout 改为文件

// cout << "FIRST" << endl;         // 写入 c.txt

// dup2(saved, STDOUT_FILENO);      // stdout 恢复终端

// cout << "SECOND" << endl;        // 输出到终端
//close(fd) 不会自动恢复标准输出。只有执行 dup2(saved, STDOUT_FILENO)，才会恢复先前保存的输出目标