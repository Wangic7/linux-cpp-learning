    #include <iostream>
    #include <unistd.h>

    using namespace std;

    int main()
    {
        pid_t pid = fork();

        if(pid == -1)
        {
            cout<<"fork failed"<<endl;
            return 1;
        }

        if(pid == 0)
        {
            cout<<"Child process"<<endl;
            cout<<"My PID: "<<getpid()<<endl;
        }
        else{
            cout<<"Parent process"<<endl;
            cout<<"My PID: "<<getpid()<<endl;
        }


        return 0;
    }