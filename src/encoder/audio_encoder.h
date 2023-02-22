/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-19 16:16:28
 * @Description:
 */
#ifndef __AUDIO_ENCODER_H__
#define __AUDIO_ENCODER_H__

#include "abstract_encoder.h"

template <>
class Encoder<MediaType::AUDIO> : public AbstractEncoder {
public:
    struct Param {
        int bitRate;
    };
    ~Encoder() { Close(); }

    bool Open(const Param& audioParma, AVFormatContext* fmtCtx);
    virtual void Close() override;
    virtual bool PushFrame(AVFrame* frame, bool isEnd, uint64_t pts) override;

private:
    bool _Init(const Param& audioParam, AVFormatContext* fmtCtx);
};

#endif