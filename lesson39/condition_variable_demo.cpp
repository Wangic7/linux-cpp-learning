#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <utility>

using namespace std;

mutex mtx;
condition_variable cv;
queue<function<void()>> tasks;
bool stop = false;

void worker()
{
    while (true) {
        function<void()> task;

        {
            unique_lock<mutex> lock(mtx);

            cv.wait(lock, [] {
                return stop || !tasks.empty();
            });

            if (stop && tasks.empty()) {
                return;
            }

            task = move(tasks.front());
            tasks.pop();
        }

        task();
    }
}

int main()
{
    vector<thread> workers;

    for (int i = 0; i < 3; ++i) {
        workers.emplace_back(worker);
    }

    {
        lock_guard<mutex> lock(mtx);

        for (int i = 1; i <= 6; ++i) {
            tasks.push([i] {
                cout << "Task " << i
                     << " executed by thread "
                     << this_thread::get_id() << endl;
            });
        }

        stop = true;
    }

    cv.notify_all();

    for (thread& t : workers) {
        t.join();
    }

    cout << "All tasks finished" << endl;

    return 0;
}

