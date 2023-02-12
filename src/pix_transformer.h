/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-03 14:47:26
 * @Description:
 */

#ifndef __PIX_TRANSFORMER_H__
#define __PIX_TRANSFORMER_H__
#include "basic.h"

extern "C" {
#include <libswscale/swscale.h>
}

template <AVPixelFormat from, AVPixelFormat to>
class PixTransformer {
public:
    bool SetSize(int width, int height)
    {
        Free(_swsCtx, [this] { sws_freeContext(_swsCtx); });
        Free(_frameTo, [this] { av_frame_free(&_frameTo); });
        // 创建格式转换
        _swsCtx = sws_getContext(
            width, height, from,
            width, height, to,
            SWS_BICUBIC, NULL, NULL, NULL);
        if (_swsCtx == nullptr) {
            __DebugPrint("sws_getContext failed\n");
            return false;
        }

        _frameTo = AllocFrame(to, width, height);
        if (_frameTo == nullptr) {
            __DebugPrint("AllocFrame failed\n");
            return false;
        }
        return true;
    }

    AVFrame* Trans(AVFrame* frameFrom)
    {
        if (frameFrom == nullptr) {
            return nullptr;
        }
        int ret = sws_scale(_swsCtx, (const uint8_t* const*)frameFrom->data,
            frameFrom->linesize, 0, frameFrom->height, _frameTo->data,
            _frameTo->linesize);

        if (ret < 0) {
            __DebugPrint("sws_scale failed\n");
            return nullptr;
        }
        _frameTo->pts = frameFrom->pts;
        return _frameTo;
    }

    ~PixTransformer()
    {
        Free(_swsCtx, [this] { sws_freeContext(_swsCtx); });
        Free(_frameTo, [this] { av_frame_free(&_frameTo); });
    }

private:
    AVFrame* _frameTo = nullptr;
    SwsContext* _swsCtx = nullptr;
};

#endif