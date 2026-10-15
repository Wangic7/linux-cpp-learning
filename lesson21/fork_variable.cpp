#include <iostream>
#include <unistd.h>

using namespace std;


int main()
{
    int number = 100;

    pid_t pid = fork();

    if(pid == -1)
    {
        cout<<"fork failed"<<endl;
        return 1;
    }

    if(pid == 0)
    {
        number = number + 50;
        cout << number << endl;
        cout << "PID: " << getpid() << endl;
    }
    else{
        number = number - 30;
        cout << number <<endl;
        cout << "PID: " << getpid() << endl;
    }


    return 0;
}