#include "mpsc_bounded_queue.hpp"

#include "callback.hpp"

#include <thread>
#include <stdexcept>
#include <functional>

class thread_pool_t::worker_t : private noncopyable_t
{
    enum {QUEUE_SIZE = 1024};
public:
    worker_t()
        : m_stop_flag(false)
        , m_thread(&thread_pool_t::worker_t::thread_func, this)
    {
    }

    void stop()
    {
        m_stop_flag = true;
        m_thread.join();
    }

    template <typename Handler>
    bool post(Handler &&handler)
    {
        return m_queue.move_push(std::forward<Handler>(handler));
    }

private:
    void thread_func()
    {
        while (!m_stop_flag)
        {
            if (callback_t *handler = m_queue.front())
            {
                (*handler)();
                m_queue.pop();
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    mpsc_bounded_queue_t<callback_t, QUEUE_SIZE> m_queue;

    bool m_stop_flag;
    std::thread m_thread;
};

inline thread_pool_t::thread_pool_t(size_t threads_count)
    : m_pool_size(threads_count)
    , m_index(0)
{
    if (AUTODETECT == m_pool_size)
        m_pool_size = std::thread::hardware_concurrency();

    if (0 == m_pool_size)
        m_pool_size = 1;

    m_pool.resize(m_pool_size);

    for (auto &worker : m_pool)
    {
        worker = new worker_t;
    }
}

inline thread_pool_t::~thread_pool_t()
{
    for (auto &worker : m_pool)
    {
        worker->stop();
        delete worker;
    }
}

template <typename Handler>
inline void thread_pool_t::post(Handler &&handler)
{
    if (!m_pool[m_index++ % m_pool_size]->post(std::forward<Handler>(handler)))
    {
        throw std::overflow_error("worker queue is full");
    }
}


