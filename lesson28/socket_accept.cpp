#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1)
    {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(server_fd,
             reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) == -1)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) == -1)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    cout << "Server listening..." << endl;

    // 在这里调用 accept()
    int client_fd = accept(server_fd,nullptr,nullptr);
    // 判断 accept() 是否失败
    if(client_fd == -1)
    {
        perror("accept");
        close(server_fd);
        return 1;
    }
    // 成功后输出：
    // Client connected
    cout<<"Client connected"<<endl;

    close(server_fd);
    close(client_fd);

    return 0;
}