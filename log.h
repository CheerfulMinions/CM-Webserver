#include <stdio.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <filesystem>
#include <format>
#include <atomic>
#include <sys/time.h>


using namespace std::literals;

class Log final
{
public:
    static Log* get_instance() noexcept
    {
        static Log instance;
        return &instance;
    }
    static void flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
    }
    //可选择的参数有日志文件名称、是否关闭缓冲区、日志缓冲区大小、最大行数以及阻塞队列最大长度
    bool init(const std::string_view file_name, bool close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    template<typename... Args>
    void write_log(int level, const std::string_view fmt, Args&&... args);

    void flush();

private:
    Log() = default;
    ~Log();

    void async_write_log();

private:
    std::string m_dir_name;
    std::string m_log_name;
    int m_split_lines;
    int m_log_buf_size;
    long long m_count;
    time_t m_today;
    FILE* m_fp = nullptr;
    char* m_buf = nullptr;
    std::unique_ptr<block_queue<std::string>> m_log_queue;   //阻塞队列
    std::unique_ptr<std::thread> m_write_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_is_async{false};
    std::atomic<bool> m_close_log{false};
};

#define LOG_DEBUG(...) if (!m_close_log.load()) { Log::get_instance().write_log(0, __VA_ARGS__); Log::get_instance().flush(); }
#define LOG_INFO(...) if (!m_close_log.load()) { Log::get_instance().write_log(1, __VA_ARGS__); Log::get_instance().flush(); }
#define LOG_WARN(...) if (!m_close_log.load()) { Log::get_instance().write_log(2, __VA_ARGS__); Log::get_instance().flush(); }
#define LOG_ERROR(...) if (!m_close_log.load()) { Log::get_instance().write_log(3, __VA_ARGS__); Log::get_instance().flush(); }

bool Log::init(const std::string_view file_name, bool close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    m_close_log.store(close_log);
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];

    struct timeval now;
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;

    m_today = mktime(&t);

    auto path = std::filesystem::path{file_name};
    m_dir_name = path.parent_path().string();
    m_log_name = path.filename().string();

    int LOG_NAME_LEN = 126;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d.log",
            m_dir_name, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

    m_fp = fopen(fileName, "a");
    if (m_fp == NULL) return false;

    m_split_lines = split_lines;
    m_count = 0;

    if (max_queue_size >= 1) {
        m_is_async.store(true);
        if(!m_log_queue) {
            unique_ptr<BlockQueue<std::string>> newDeque(new BlockQueue<std::string>);
            m_log_queue = std::move(newDeque);
            
            std::unique_ptr<std::thread> NewThread(new std::thread(&Log::async_write_log, this));
            m_write_thread = move(NewThread);
        }
    }
    return true;
}

Log::~Log()
{
    delete[] m_buf;
    if (m_fp)
    {
        fclose(m_fp);
    }
}

template<typename... Args>
void Log::write_log(int level, const std::string_view fmt, Args&&... args)
{
    std::scoped_lock lock{m_mutex};

    struct timeval now;
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;

    time_t today = mktime(&t);

    if (today != m_today || m_count % m_split_lines == 0)
    {
        std::string new_log_name = [&]() {
            std::stringstream ss;
            ss << m_dir_name << "/" << today << "_" << m_log_name;
            if (today != m_today)
            {
                m_count = 0;
            }
            else
            {
                ss << "." << m_count / m_split_lines;
            }
            return ss.str();
        }();

        fclose(m_fp);
        m_today = today;
        m_fp = fopen(new_log_name.c_str(), "a");
    }

    m_count++;

    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &t);

    std::string log_str = [&]() {
        std::stringstream ss;
        ss << "[" << time_buf << "] ";
        switch (level)
        {
        case 0:
            ss << "[debug]: ";
            break;
        case 1:
            ss << "[info]: ";
            break;
        case 2:
            ss << "[warn]: ";
            break;
        case 3:
            ss << "[error]: ";
            break;
        default:
            ss << "[info]: ";
            break;
        }
        ss << std::format(fmt, std::forward<Args>(args)...) << '\n';
        return ss.str();
    }();

    if (m_is_async.load())
    {
        m_log_queue.push(log_str);
    }
    else
    {
        fputs(log_str.c_str(), m_fp);
    }
}

void Log::async_write_log(){
    while (true){
        std::unique_lock lock{m_mutex};
        m_cv.wait(lock, [this]{ return !m_log_queue.empty() || m_close_log.load(); });

        if (m_close_log.load())
            break;

        while (!m_log_queue.empty())
        {
            const auto& log_str = m_log_queue.front();
            fputs(log_str.c_str(), m_fp);
            m_log_queue.pop();
        }

        fflush(m_fp);
    }
}

void Log::flush(){
    if(m_is_async.load()) {
        m_log_queue->flush(); 
    }
    fflush(fp_);
}
