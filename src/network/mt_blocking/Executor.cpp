#include <afina/Executor.h>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace Afina {

// See Executor.h
Executor::Executor(std::string name, int low_watermark, int hight_watermark, int max_queue_size, int idle_time) : _low_watermark(low_watermark), _hight_watermark(hight_watermark), _max_queue_size(max_queue_size), _idle_time(idle_time) {
    _state.store(State::kReady);
}

// See Executor.h
Executor::~Executor() {}

// See Executor.h
void Executor::Stop(bool await = false) {
    state = State::kStopping;
    std::unique_lock<std::mutex> lock(mutex);
    while (tasks.size() > 0) {
        empty_condition.notify_one();
    }
    if (await) {
        while (state == State::kStopping) {
            stop_condition.wait(lock);
        }
    }
}

// See Executor.h
void Executor::Start() {
    std::unique_lock<std::mutex> lock(mutex);
    state = State::kRun;
    for (int i = 0; i < lower_watermark; i++) {
        auto id = std::thread::id::id();
        std::unique_ptr<std::thread> new_thread(&thread_run, this, id);
        threads.push_back(std::move(new_thread));
        threads.back()->detach();
    }
    idle_threads = lower_watermark;
}

void Executor::Delete_thread_by_id(Executor *executor, std::thread::id id) {
    auto it = std::find_if(executor->threads.begin(), executor->threads.end(), [&](std::unique_ptr<std::thread>& obj){return obj->get_id() == id;});
    assert(it != executor->threads.end());
    executor->threads.erase(it);
}

void Executor::thread_run(Executor* executor, std::thread::id id) {
    while (state == State::kRun) {
        auto start = std::chrono::system_clock::now();
        std::unique_lock<std::mutex> lock(executor->mutex);
        executor->idle_threads++;

        while (executor->tasks.empty()) {
            empty_condition.wait_until(lock, start + (executor->idle_time)*100ms);

            auto end = std::chrono::system_clock::now();
            int elapsed = std::chrono::duration_cast<std::chrono::milliseconds> (end - start).count();

                if ((elapsed >= executor->idle_time) && (executor->threads.size() > executor->lower_watermark)) {
                executor->Delete_thread_by_id(executor, id);
                return;
            }
            start = std::chrono::system_clock::now();
        }

        executor->idle_threads--;
        
        auto task = executor->tasks.back();
        executor->tasks.pop_back();

        lock.unlock();
        task();
        lock.lock();
        
        if (executor->state == State::kStopping) {
            executor->Delete_thread_by_id(executor, id);
            if (executor->threads.size() == 0) {
                executor->state = State::kStopped;
                executor->stop_condition.notify_one();
            }
            return;
        }
    }
}

} // namespace Afina
