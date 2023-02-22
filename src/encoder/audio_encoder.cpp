/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-19 16:20:16
 * @Description:
 */
#include "audio_encoder.h"

bool Encoder<MediaType::AUDIO>::Open(const Param& audioParma, AVFormatContext* fmtCtx)
{
    Close();
    _isOpen = false;
    __CheckBool(_Init(audioParma, fmtCtx));
    __CheckBool(avcodec_open2(_codecCtx, _codec, nullptr) >= 0);
    _isOpen = true;
    return true;
}
void Encoder<MediaType::AUDIO>::Close()
{
    if (_codecCtx != nullptr) {
        avcodec_close(_codecCtx);
    }
    Free(_codecCtx, [this] { avcodec_free_context(&_codecCtx); });
}
bool Encoder<MediaType::AUDIO>::_Init(const Param& audioParam, AVFormatContext* fmtCtx)
{
    // codec
    __CheckBool(_codec = avcodec_find_encoder(fmtCtx->oformat->audio_codec));
    // codeccontext
    __CheckBool(_codecCtx = avcodec_alloc_context3(_codec));
    _codecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    _codecCtx->bit_rate = audioParam.bitRate;
    _codecCtx->sample_rate = AUDIO_SAMPLE_RATE;
    auto x = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
    av_channel_layout_copy(&_codecCtx->ch_layout, &x);
    if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        _codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    return true;
}

bool Encoder<MediaType::AUDIO>::PushFrame(AVFrame* frame, bool isEnd, uint64_t pts)
{
    if (!isEnd) {
        __CheckBool(frame);
    } else {
        frame = nullptr;
    }
    if (frame != nullptr) {
        frame->pts = pts;
    }
    __CheckBool(avcodec_send_frame(_codecCtx, frame) >= 0);
    return true;
}
