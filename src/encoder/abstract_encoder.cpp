/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-19 16:27:59
 * @Description:
 */
#include "abstract_encoder.h"

AVPacket* AbstractEncoder::Encode()
{
    int ret = avcodec_receive_packet(_codecCtx, _packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return nullptr;
    } else if (ret < 0) {
        __DebugPrint("avcodec_receive_packet : Error during encoding");
        return nullptr;
    }
    return _packet;
}