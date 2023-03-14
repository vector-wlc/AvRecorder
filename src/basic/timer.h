/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-06 10:37:38
 * @Description:
 */
#ifndef __TIMER_H__
#define __TIMER_H__

#include "basic/basic.h"
#include <Windows.h>
#include <functional>

class Timer {
public:
    ~Timer()
    {
        Stop();
    }

    // interval 为 0 表示时刻执行
    template <typename Func>
    void Start(int fps, Func&& func)
    {
        static_assert(std::is_convertible_v<Func, std::function<void()>>, "func need to be std::function<void()>");
        _fps = fps;
        _tickCnt = 0;
        _isOverload = false;
        __CheckNo(!_isRunning);
        using namespace std::chrono;
        _isRunning = true;
        _beginTime = high_resolution_clock::now();
        if (_fps > 0) {
            auto task = [this, func = std::forward<Func>(func)]() mutable {
                while (_isRunning) {
                    // 这里不能直接使用整数除法
                    // 因为整数除法有截断，导致最终睡眠的时间少一些
                    uint64_t goalTime = int((double(1000) / _fps * _tickCnt) + 0.5);
                    ++_tickCnt;
                    auto nowTime = high_resolution_clock::now();
                    auto duration = duration_cast<milliseconds>(nowTime - _beginTime).count();
                    int64_t sleepTime = goalTime - duration;
                    if (sleepTime > 0) {
                        SleepMs(sleepTime);
                    }
#ifdef __AVDEBUG
                    // else if (sleepTime < 0) {
                    //     printf("Time out : %lld\n", -sleepTime);
                    // }
#endif
                    _isOverload = -sleepTime > 1000; // 捕获的过载时间设置为 1s
                    func();
                }
            };
            _thread = new std::thread(std::move(task));
            timeBeginPeriod(1);
            return;
        }

        auto task = [this, func = std::forward<Func>(func)]() mutable {
            while (_isRunning) {
                func();
            }
        };
        _thread = new std::thread(std::move(task));
    }

    void Stop()
    {
        _isRunning = false;
        if (_thread == nullptr) {
            return;
        }
        if (_fps > 0) {
            timeEndPeriod(1);
        }
        _thread->join();
        delete _thread;

        _thread = nullptr;
    }

    bool IsOverload() const { return _isOverload; }

private:
    int _fps = 100;
    int _isRunning = false;
    int _isOverload = false;
    std::vector<int> vec;
    std::chrono::time_point<std::chrono::high_resolution_clock> _beginTime;
    std::thread* _thread = nullptr;
    uint64_t _tickCnt = 0;
};

#endif