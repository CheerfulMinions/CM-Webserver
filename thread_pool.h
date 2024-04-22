#pragma once
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include <assert.h>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t thread_count = 8)
        : m_pool{std::make_shared<Pool>()}
    {
        assert(thread_count > 0);
        for (std::size_t i = 0; i < thread_count; ++i) {
            std::thread{[this_pool = m_pool] {
                std::unique_lock lock{this_pool->mtx};
                while (true) {
                    if (!this_pool->tasks.empty()) {
                        auto task = std::move(this_pool->tasks.front());
                        this_pool->tasks.pop();
                        lock.unlock();
                        task();
                        lock.lock();
                    } else if (this_pool->is_closed) break;
                    else this_pool->cond.wait(lock);
                }
            }}.detach();
        }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;

    ~ThreadPool()
    {
        if (static_cast<bool>(m_pool)) {
            {
                std::lock_guard lock{m_pool->mtx};
                m_pool->is_closed = true;
            }
            m_pool->cond.notify_all();
        }
    }

    template <typename F>
    void add_task(F&& task)
    {
        {
            std::lock_guard lock{m_pool->mtx};
            m_pool->tasks.emplace(std::forward<F>(task));
        }
        m_pool->cond.notify_one();
    }

private:
    struct Pool {
        std::mutex mtx;
        std::condition_variable cond;
        bool is_closed;
        std::queue<std::function<void()>> tasks;
    };

    std::shared_ptr<Pool> m_pool;
};
