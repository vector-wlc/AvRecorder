#ifndef __FRAME_H__
#define __FRAME_H__
#include "basic/basic.h"

class __BasicFrame {
public:
    AVFrame* frame = nullptr;
    __BasicFrame() = default;
    __BasicFrame(__BasicFrame&& rhs) noexcept
    {
        frame = rhs.frame;
        rhs.frame = nullptr;
    }
    __BasicFrame& operator=(__BasicFrame&& rhs)
    {
        Free(frame, [this] { av_frame_free(&frame); });
        frame = rhs.frame;
        rhs.frame = nullptr;
        return *this;
    }
    __BasicFrame(const __BasicFrame& rhs) = delete;
    __BasicFrame& operator=(const __BasicFrame& rhs) = delete;
    ~__BasicFrame()
    {
        Free(frame, [this] { av_frame_free(&frame); });
    }
};

template <MediaType mediaType>
class Frame;

template <>
class Frame<MediaType::AUDIO> : public __BasicFrame {
public:
    static AVFrame* Alloc(AVSampleFormat sampleFmt,
        const AVChannelLayout* channel_layout,
        int sampleRate, int nbSamples);

    Frame(AVSampleFormat sampleFmt,
        const AVChannelLayout* channel_layout, int sampleRate,
        int nbSamples);

    Frame(AVFrame* frame);
};

template <>
class Frame<MediaType::VIDEO> : public __BasicFrame {
public:
    static AVFrame* Alloc(AVPixelFormat pixFmt, int width, int height);
    Frame(AVPixelFormat pixFmt, int width, int height);
    Frame(AVFrame* frame);
};

struct SwsContext;

class PixTransformer {
private:
    AVPixelFormat _from;
    AVPixelFormat _to;

public:
    PixTransformer(AVPixelFormat from, AVPixelFormat to)
        : _from(from)
        , _to(to)
    {
    }
    bool SetSize(int width, int height);
    AVFrame* Trans(AVFrame* frameFrom);
    ~PixTransformer();

private:
    AVFrame* _frameTo = nullptr;
    SwsContext* _swsCtx = nullptr;
};

#endif