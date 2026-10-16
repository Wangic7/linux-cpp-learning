#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>//要使用strlen(response)

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

    int opt = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

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

    while(true)
    {
        int client_fd = accept(server_fd,nullptr,nullptr);
   
        if(client_fd == -1)
        {
            perror("accept");
            close(server_fd);
            return 1;
        }

        cout<<"Client connected"<<endl;

        char buffer[128];
        ssize_t n =read(client_fd,buffer,sizeof(buffer) - 1);
        if(n == -1)
        {
            perror("read");
            close(client_fd);
            close(server_fd);
            return 1;
        }

        buffer[n] = '\0';

        cout << "Received: " << buffer << endl;

        const char* response = "Hello Client";
        n = write(client_fd,response,strlen(response));
        if(n == -1)
        {
            perror("write");
            close(client_fd);
            close(server_fd);
            return 1;
        }

        close(client_fd);
    }

    close(server_fd);

    return 0;

}