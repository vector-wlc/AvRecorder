/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-19 14:23:39
 * @Description:
 */
#include "basic/frame.h"

extern "C" {
#include <libswscale/swscale.h>
}

AVFrame* Frame<MediaType::AUDIO>::Alloc(AVSampleFormat sampleFmt,
    const AVChannelLayout* channel_layout,
    int sampleRate, int nbSamples)
{
    AVFrame* frame;
    __CheckNullptr(frame = av_frame_alloc());
    frame->format = sampleFmt;
    av_channel_layout_copy(&frame->ch_layout, channel_layout);
    frame->sample_rate = sampleRate;
    frame->nb_samples = nbSamples;

    /* allocate the buffers for the frame data */
    __CheckNullptr(av_frame_get_buffer(frame, 0) >= 0);
    return frame;
}

Frame<MediaType::AUDIO>::Frame(AVSampleFormat sampleFmt,
    const AVChannelLayout* channel_layout, int sampleRate,
    int nbSamples)
{
    __CheckNo(frame = Alloc(sampleFmt, channel_layout, sampleRate, nbSamples));
}

Frame<MediaType::AUDIO>::Frame(AVFrame* frame)
{
    if (frame == nullptr) {
        this->frame = nullptr;
        return;
    }
    __CheckNo(this->frame = Alloc(AVSampleFormat(frame->format), &frame->ch_layout, frame->sample_rate, frame->nb_samples));
    __CheckNo(av_frame_copy(this->frame, frame) >= 0);
}

Frame<MediaType::VIDEO>::Frame(AVPixelFormat pixFmt, int width, int height)
{
    __CheckNo(frame = Alloc(pixFmt, width, height));
}

AVFrame* Frame<MediaType::VIDEO>::Alloc(AVPixelFormat pixFmt, int width, int height)
{
    AVFrame* frame = nullptr;
    __CheckNullptr(frame = av_frame_alloc());

    frame->format = pixFmt;
    frame->width = width;
    frame->height = height;

    /* allocate the buffers for the frame data */
    __CheckNullptr(av_frame_get_buffer(frame, 0) >= 0);
    return frame;
}

Frame<MediaType::VIDEO>::Frame(AVFrame* frame)
{
    if (frame == nullptr) {
        this->frame = nullptr;
        return;
    }
    __CheckNo(this->frame = Alloc(AVPixelFormat(frame->format), frame->width, frame->height));
    __CheckNo(av_frame_copy(this->frame, frame) >= 0);
}

bool PixTransformer::SetSize(int width, int height)
{
    Free(_swsCtx, [this] { sws_freeContext(_swsCtx); });
    Free(_frameTo, [this] { av_frame_free(&_frameTo); });
    // 创建格式转换
    __CheckBool(_swsCtx = sws_getContext(
                    width, height, _from,
                    width, height, _to,
                    0, NULL, NULL, NULL));

    __CheckBool(_frameTo = Frame<MediaType::VIDEO>::Alloc(_to, width, height));
    return true;
}

AVFrame* PixTransformer::Trans(AVFrame* frameFrom)
{
    // 如果是空指针，直接把缓存返回
    if (frameFrom == nullptr) {
        return _frameTo;
    }
    __CheckNullptr(
        sws_scale(_swsCtx, (const uint8_t* const*)frameFrom->data,
            frameFrom->linesize, 0, frameFrom->height, _frameTo->data,
            _frameTo->linesize)
        >= 0);
    _frameTo->pts = frameFrom->pts;
    return _frameTo;
}

PixTransformer::~PixTransformer()
{
    Free(_swsCtx, [this] { sws_freeContext(_swsCtx); });
    Free(_frameTo, [this] { av_frame_free(&_frameTo); });
}
