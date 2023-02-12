/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-08 16:38:26
 * @Description:
 */
#ifndef __MUTEX_QUEUE_H__
#define __MUTEX_QUEUE_H__

#include <mutex>
#include <queue>

template <typename T>
class MutexQueue {
public:
    using __LockMtx = std::lock_guard<std::mutex>;

    void Push(const T& t)
    {
        // __LockMtx lm(_mtx);
        _qu.push(t);
    }
    void Push(T&& t)
    {
        // __LockMtx lm(_mtx);
        _qu.push(std::move(t));
    }

    T Pop()
    {
        // __LockMtx lm(_mtx);
        auto ret = std::move(_qu.front());
        _qu.pop();
        return ret;
    }

    T& Back()
    {
        // __LockMtx lm(_mtx);
        auto&& ret = _qu.back();
        return ret;
    }

    bool IsEmpty()
    {
        // __LockMtx lm(_mtx);
        auto ret = _qu.empty();
        return ret;
    }

private:
    std::queue<T> _qu;
    std::mutex _mtx;
};

#endif