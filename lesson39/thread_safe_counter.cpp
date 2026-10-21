
#include <iostream>
#include <thread>
#include <mutex>

using namespace std;

int counter = 0;
mutex mtx;

void worker(int times)
{
    for (int i = 0; i < times; ++i) {
        lock_guard<mutex> lock(mtx);
        ++counter;//每次循环都先获取锁，再修改 counter。本轮循环结束、lock 对象离开作用域时，锁自动释放，下一轮再获取
    }
}

int main()
{
    thread t1(worker, 100000);
    thread t2(worker, 100000);

    t1.join();
    t2.join();

    cout << "Final counter: " << counter << endl;

    return 0;
}
