/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-01-28 11:01:58
 * @Description:
 */
#ifndef __BASIC_FUCN_H__
#define __BASIC_FUCN_H__
#define __STDC_FORMAT_MACROS

#include <cstdio>
#include <functional>
#include <mutex>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

// ***************
// MUTEX
extern std::mutex __mtx;

// ***************
// debug function

#define __AVDEBUG

#ifdef __AVDEBUG
// #define __DebugPrint(fmtStr, ...)
#define __DebugPrint(fmtStr, ...) \
    std::printf("[" __FILE__ ", line:%d] " fmtStr "\n", __LINE__, ##__VA_ARGS__)

#define __Str(exp) #exp
#define __Check(retVal, ...)                            \
    do {                                                \
        if (!(__VA_ARGS__)) {                           \
            __DebugPrint(__Str(__VA_ARGS__) " failed"); \
            return retVal;                              \
        }                                               \
    } while (false)

#define __CheckNo(...) __Check(, __VA_ARGS__)
#define __CheckBool(...) __Check(false, __VA_ARGS__)
#define __CheckNullptr(...) __Check(nullptr, __VA_ARGS__)

#else
#define __DebugPrint(fmtStr, ...)
#define __Check(retVal, ...)
#define __CheckNo(...)
#define __CheckBool(...)
#define __CheckNullptr(...)
#endif

enum class MediaType {
    AUDIO,
    VIDEO
};

// ***************
// memory function

template <typename T, typename Func>
void Free(T*& ptr, Func&& func)
{
    static_assert(std::is_convertible_v<Func, std::function<void()>>, "Type Func should be std::function<void()>");
    if (ptr == nullptr) {
        return;
    }

    func();
    ptr = nullptr;
}

//***************
// time function

// Sleep x ms
inline void SleepMs(int timeMs)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(timeMs));
}

// 对于音频编码器的全局设置
constexpr int AUDIO_SAMPLE_RATE = 44100;
constexpr int AUDIO_CHANNEL = 1;
constexpr AVSampleFormat AUDIO_FMT = AV_SAMPLE_FMT_FLTP;
constexpr int MICROPHONE_INDEX = 0;
constexpr int SPEAKER_INDEX = 1;

#endif