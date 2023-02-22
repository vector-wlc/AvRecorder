/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-19 15:41:37
 * @Description:
 */
#ifndef __BASIC_ENCODER_H__
#define __BASIC_ENCODER_H__

#include "basic/basic.h"

class AbstractEncoder {
public:
    AbstractEncoder()
    {
        _packet = av_packet_alloc();
    }
    AVCodecContext* GetCtx() const
    {
        return _codecCtx;
    }

    virtual bool PushFrame(AVFrame* frame, bool isEnd, uint64_t pts) = 0;
    AVPacket* Encode();
    virtual void AfterEncode() {};
    virtual void Close() = 0;
    virtual ~AbstractEncoder()
    {
        Free(_packet, [this] { av_packet_free(&_packet); });
    }

protected:
    bool _isOpen = false;
    AVPacket* _packet = nullptr;
    const AVCodec* _codec = nullptr;
    AVCodecContext* _codecCtx = nullptr;
};

template <MediaType mediaType>
class Encoder;

#endif