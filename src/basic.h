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

// #define __DebugPrint(fmtStr, ...)
#define __DebugPrint(fmtStr, ...)                  \
    std::printf("[" __FILE__ ", line:%d] " fmtStr, \
        __LINE__, ##__VA_ARGS__)

extern std::mutex __mtx;
using LockGuardMtx = std::lock_guard<std::mutex>;
#define __LockGuard()                \
    /* __DebugPrint("Try lock\n");*/ \
    LockGuardMtx lg(__mtx)

template <typename T, typename Func>
void Free(T*& ptr, Func&& func)
{
    static_assert(std::is_convertible_v<Func, std::function<void()>>, "Type Func should be std::function<void()>");
    if (ptr == nullptr) {
        // std::printf("nullptr can't be freed\n");
        return;
    }

    func();
    ptr = nullptr;
}

struct AvPacket {
    AVPacket* packet;
    int streamIndex;
    AvPacket()
    {
        packet = av_packet_alloc();
        if (packet == nullptr) {
            __DebugPrint("av_packet_alloc failed\n");
        }
    }
    AvPacket(AvPacket&& rhs) noexcept
    {
        packet = rhs.packet;
        rhs.packet = nullptr;
    }
    AvPacket(const AvPacket& rhs) = delete;
    ~AvPacket()
    {
        Free(packet, [this] { av_packet_free(&packet); });
    }
};

inline AVFrame* AllocFrame(enum AVPixelFormat pixFmt, int width, int height)
{
    AVFrame* frame;
    int ret;

    frame = av_frame_alloc();
    if (!frame) {
        __DebugPrint("av_frame_alloc failed\n");
        return nullptr;
    }

    frame->format = pixFmt;
    frame->width = width;
    frame->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        __DebugPrint("av_frame_get_buffer failed\n");
        return nullptr;
    }
    return frame;
}

struct AvFrame {
    AVFrame* frame = nullptr;
    int streamIndex = 0;
    uint64_t id = 0;
    AvFrame(enum AVPixelFormat pixFmt, int width, int height)
    {
        frame = AllocFrame(pixFmt, width, height);
        if (this->frame == nullptr) {
            __DebugPrint("AllocFrame failed\n");
            return;
        }
    }

    AvFrame(AVFrame* frame)
    {
        if (frame == nullptr) {
            this->frame = nullptr;
            return;
        }
        this->frame = AllocFrame(AVPixelFormat(frame->format), frame->width, frame->height);
        if (this->frame == nullptr) {
            __DebugPrint("AllocFrame failed\n");
            return;
        }
        if (av_frame_copy(this->frame, frame) < 0) {
            __DebugPrint("av_frame_copy failed\n");
            return;
        }
    }
    AvFrame(AvFrame&& rhs) noexcept
    {
        frame = rhs.frame;
        streamIndex = rhs.streamIndex;
        id = rhs.id;
        rhs.frame = nullptr;
    }
    AvFrame& operator=(AvFrame&& rhs)
    {
        Free(frame, [this] { av_frame_free(&frame); });
        frame = rhs.frame;
        streamIndex = rhs.streamIndex;
        id = rhs.id;
        rhs.frame = nullptr;
        return *this;
    }
    AvFrame(const AvFrame& rhs) = delete;
    AvFrame& operator=(const AvFrame& rhs) = delete;
    ~AvFrame()
    {
        Free(frame, [this] { av_frame_free(&frame); });
    }
};

// Sleep x ms
inline void SleepMs(int timeMs)
{
    std::this_thread::sleep_for(std::chrono::microseconds(timeMs * 1000));
}

// Sleep 1 ms
inline void SleepALitteTime()
{
    SleepMs(1);
}

#endif