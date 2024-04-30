/*
 * 只有在异步写时，才会用到
 * m_data是循环数组
*/
#pragma once

#include <array>
#include <condition_variable>
#include <mutex>
#include <optional>

template <typename T, std::size_t N>
class block_queue
{
public:
    block_queue() = default;
    ~block_queue(){
        std::lock_guard lock{m_mutex};
        m_is_close = true;
        m_cond.notify_all();
    }

    void clear() noexcept
    {
        std::unique_lock lock{m_mutex};
        m_data.fill({});
        m_size = 0;
        m_front = m_back = 0;
    }

    bool full() const noexcept {
        std::lock_guard lock{m_mutex};
        return m_size >= N;
    }

    bool empty() const noexcept
    {
        std::lock_guard lock{m_mutex};
        return m_size == 0;
    }

    std::optional<T> front() noexcept
    {
        std::unique_lock lock{m_mutex};
        return m_size == 0 ? std::nullopt : std::make_optional(m_data[m_front]);
    }

    std::optional<T> back() noexcept
    {
        std::unique_lock lock{m_mutex};
        return m_size == 0 ? std::nullopt : std::make_optional(m_data[m_back]);
    }

    std::size_t size() const noexcept
    {
        std::lock_guard lock{m_mutex};
        return m_size;
    }

    std::size_t max_size() const noexcept
    {
        return N;
    }

    bool push(const T& item) {
        std::unique_lock lock{m_mutex};
        if (m_size >= N) {
            m_cond.notify_all();
            return false;
        }

        m_data[m_back] = item;
        m_back = (m_back + 1) % N;
        ++m_size;
        m_cond.notify_all();
        return true;
    }

    bool pop(T& item) {
        std::unique_lock lock{m_mutex};
        while(m_size <= 0) {
            m_cond.wait(lock);
            if(m_is_close) break;
        }

        item = std::move(m_data[m_front]);
        m_front = (m_front + 1) % N;
        --m_size;
        return true;
    }

    bool pop(T& item, std::chrono::milliseconds timeout = std::chrono::milliseconds::zero())
    {
        std::unique_lock lock{m_mutex};
        if (m_size <= 0) {
            if (timeout.count() > 0 && !m_cond.wait_for(lock, timeout, [this]{ return !empty(); }))
                return false;
        }

        item = std::move(m_data[m_front]);
        m_front = (m_front + 1) % N;
        --m_size;
        return true;
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    // 循环数组
    std::array<T, N> m_data;
    std::size_t m_size = 0;
    std::size_t m_front = 0;
    std::size_t m_back = 0;

    bool m_is_close = false;
};



/*
`std::array` 是 C++ 标准库中提供的固定大小的数组容器。与 `std::vector` 不同，`std::array` 在编译时即确定其大小，并且其内部数据存储在栈上或者作为类成员时存储在对象所在的内存区域。
由于其大小固定且非动态分配，`std::array` 不需要手动释放内存。

一旦 `std::array` 的生命周期结束（例如，当它是一个局部变量，函数返回时；或作为对象成员，对象被销毁时），其所占用的内存会自动由系统回收。不像 `std::vector` 那样在堆上动态分配元素存储空间。
*/
