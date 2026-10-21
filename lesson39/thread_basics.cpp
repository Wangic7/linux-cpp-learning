#include <iostream>
#include <thread>

using namespace std;

void worker()
{
    cout << "Worker thread is running" << endl;
}

int main()
{
    cout << "Main: before creating thread" << endl;

    thread t(worker);//写worker:将函数交给线程，让新线程执行它;写worker():直接调用函数，让当前线程执行它。这是两种完全不同的操作

    cout << "Main: thread started" << endl;

    t.join();//等待线程完成，join() 会阻塞调用它的线程，直到 t 所代表的工作线程执行结束

    cout << "Main: worker finished" << endl;

    return 0;
}
