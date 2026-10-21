
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>
#include <utility>

using namespace std;
mutex outputMtx;

class ThreadPool
{
private:
    mutex mtx_;
    condition_variable cv_;
    queue<function<void()>> tasks_;
    vector<thread> workers_;
    bool stop_ = false;

    void worker()
    {
        while (true)
        {
            function<void()> task;

            {
                unique_lock<mutex> lock(mtx_);

                cv_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });

                if (stop_ && tasks_.empty()) {
                    return;
                }

                task = move(tasks_.front());
                tasks_.pop();
            }

            task();
        }
    }

public:
    explicit ThreadPool(size_t threadCount)
    {
        if (threadCount == 0)
        {
            throw invalid_argument(
                "threadCount must be greater than 0");
        }

        for (size_t i = 0; i < threadCount; ++i)
        {
            workers_.emplace_back([this] {
                worker();
            });
        }
    }

    void addTask(function<void()> task)
    {
        {
            lock_guard<mutex> lock(mtx_);
            tasks_.push(move(task));
        }

        cv_.notify_one();
    }

    ~ThreadPool()
    {
        {
            lock_guard<mutex> lock(mtx_);
            stop_ = true;
        }

        cv_.notify_all();

        for (thread& t : workers_)
        {
            t.join();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
};

int main()
{
    ThreadPool pool(3);

    for (int i = 1; i <= 6; ++i)
    {
       pool.addTask([i] {
            lock_guard<mutex> lock(outputMtx);

            cout << "Task " << i
                << " executed by thread "
                << this_thread::get_id() << endl;
        });
    }

    {
    lock_guard<mutex> lock(outputMtx);
    cout << "Tasks submitted" << endl;
    }

    return 0;
}

