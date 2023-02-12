/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-06 10:37:38
 * @Description:
 */
#ifndef __TIMER_H__
#define __TIMER_H__

#include "basic.h"
#include <functional>

#include <Windows.h>

class Timer {
public:
    ~Timer()
    {
        Stop();
    }

    // interval 为 0 表示时刻执行
    template <typename Func>
        requires std::is_convertible_v<Func, std::function<void()>>
    void Start(int interval, Func&& func)
    {
        _interval = interval;
        _tickCnt = 0;

        if (_isRunning) {
            __DebugPrint("Can't start again\n");
            return;
        }
        using namespace std::chrono;
        _isRunning = true;
        _beginTime = high_resolution_clock::now();
        if (interval > 0) {
            auto task = [this, func = std::forward<Func>(func)]() mutable {
                while (_isRunning) {
                    ++_tickCnt;
                    auto nowTime = high_resolution_clock::now();
                    auto duration = duration_cast<milliseconds>(nowTime - _beginTime).count();
                    int64_t sleepTime = _interval * _tickCnt - duration;
                    if (sleepTime > 0) {
                        SleepMs(sleepTime);
                    } else if (sleepTime < 0) {
                        printf("Time out : %lld\n", -sleepTime);
                    }
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
        if (_interval > 0) {
            timeEndPeriod(1);
        }
        _thread->join();
        delete _thread;

        _thread = nullptr;
    }

private:
    int _interval = 100;
    int _isRunning = false;
    std::vector<int> vec;
    std::chrono::time_point<std::chrono::high_resolution_clock> _beginTime;
    std::thread* _thread = nullptr;
    uint64_t _tickCnt = 0;
};

#endif