/******************************************************************************

                              Online C++ Compiler.
               Code, Compile, Run and Debug C++ program online.
Write your code in this editor and press "Run" button to compile and execute it.

*******************************************************************************/
#include <queue>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <chrono>
#include <vector>
#include <assert.h>

using TimeoutCallBack = std::function<void()>;
using TimeStamp = std::chrono::time_point<std::chrono::system_clock>;
using MS = std::chrono::milliseconds;

struct TimerNode {
    int m_id;
    TimeStamp m_expires;        // 设置过期时间
    TimeoutCallBack m_cb;       // 回调函数
    bool operator<(const TimerNode& t) const noexcept { return m_expires < t.m_expires; }
};

class HeapTimer {
public:
    HeapTimer() noexcept = default;
    ~HeapTimer() noexcept { clear(); }

    void adjust(int id, int newExpires) noexcept;
    void add(int id, int timeOut, const TimeoutCallBack& cb) noexcept;
    void doWork(int id) noexcept;
    void clear() noexcept;
    void tick() noexcept;
    int GetNextTick() noexcept;

private:
    void del_(size_t i) noexcept;
    void pop() noexcept;
    void siftup_(size_t i) noexcept;
    bool siftdown_(size_t index, size_t n) noexcept;
    void SwapNode_(size_t i, size_t j) noexcept;

    std::vector<TimerNode> m_heap;
    std::unordered_map<int, size_t> m_ref;
};

void HeapTimer::siftup_(size_t i) noexcept {
    size_t j = (i - 1) / 2;
    while(j >= 0 && m_heap[j] < m_heap[i]) {
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) noexcept {
    std::swap(m_heap[i], m_heap[j]);
    m_ref[m_heap[i].m_id] = i;
    m_ref[m_heap[j].m_id] = j;
}

bool HeapTimer::siftdown_(size_t index, size_t n) noexcept {
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && m_heap[j + 1] < m_heap[j]) j++;
        if(m_heap[i] < m_heap[j]) break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) noexcept {
    size_t i;
    if(m_ref.count(id) == 0) {
        i = m_heap.size();
        m_ref[id] = i;
        m_heap.push_back({id, std::chrono::system_clock::now() + MS(timeout), cb});
        siftup_(i);
    } else {
        i = m_ref[id];
        m_heap[i].m_expires = std::chrono::system_clock::now() + MS(timeout);
        m_heap[i].m_cb = cb;
        if(!siftdown_(i, m_heap.size())) {
            siftup_(i);
        }
    }
}

void HeapTimer::doWork(int id) noexcept {
    if(m_heap.empty() || m_ref.count(id) == 0) return;
    size_t i = m_ref[id];
    TimerNode node = m_heap[i];
    node.m_cb();
    del_(i);
}

void HeapTimer::del_(size_t index) noexcept {
    size_t i = index;
    size_t n = m_heap.size() - 1;
    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    m_ref.erase(m_heap.back().m_id);
    m_heap.pop_back();
}

void HeapTimer::adjust(int id, int timeout) noexcept {
    m_heap[m_ref[id]].m_expires = std::chrono::system_clock::now() + MS(timeout);
    siftdown_(m_ref[id], m_heap.size());
}

void HeapTimer::pop() noexcept {
    assert(!m_heap.empty());
    del_(0);
}

void HeapTimer::tick() noexcept {
    if(m_heap.empty()) return;
    while(!m_heap.empty()) {
        TimerNode node = m_heap.front();
        if(std::chrono::duration_cast<MS>(node.m_expires - std::chrono::system_clock::now()).count() > 0) {
            break;
        }
        node.m_cb();
        pop();
    }
}

void HeapTimer::clear() noexcept {
    m_ref.clear();
    m_heap.clear();
}

int HeapTimer::GetNextTick() noexcept {
    tick();
    if(m_heap.empty()) return -1;
    auto duration = std::chrono::duration_cast<MS>(m_heap.front().m_expires - std::chrono::system_clock::now());
    return duration.count() > 0 ? duration.count() : 0;
}