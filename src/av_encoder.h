/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-03 10:01:33
 * @Description:
 */
#ifndef __VIDEO_ENCODER_H__
#define __VIDEO_ENCODER_H__

#include "basic.h"
#include "pix_transformer.h"
#include "src/basic.h"
#include <cstddef>
#include <cstdint>

extern "C" {
#include <libavutil/opt.h>
}
#endif

struct EncodeParam {
    int bitRate;
    int width;
    int height;
    int fps;
    AVFormatContext* fmtCtx = nullptr;
    std::string name;
};

class AvEncoder {
public:
    struct EncodeInfo {
        int size;
        std::vector<AvPacket> data;
    };

    ~AvEncoder()
    {
        Close();
    }

    bool Open(const EncodeParam& encodeParam)
    {
        Close();
        _pts = 0;
        _isOpen = false;
        if (!_Init(encodeParam)) {
            __DebugPrint("_Init failed\n");
            return false;
        }
        // 打开编码器
        if (avcodec_open2(_codecCtx, _codec, nullptr) < 0) {
            __DebugPrint("avcodec_open2 failed\n");
            return false;
        }

        if (!_isHardware) {
            if (!_rgbToYuv420.SetSize(encodeParam.width, encodeParam.height)) {
                __DebugPrint("_rgbToYuv420.SetSize failed\n");
                return false;
            }

            if (!_xrgbToYuv420.SetSize(encodeParam.width, encodeParam.height)) {
                __DebugPrint("_xrgbToYuv420.SetSize failed\n");
                return false;
            }
        } else {
            if (!_rgbToNv12.SetSize(encodeParam.width, encodeParam.height)) {
                __DebugPrint("_rgbToNv12.SetSize failed\n");
                return false;
            }
            if (!_xrgbToNv12.SetSize(encodeParam.width, encodeParam.height)) {
                __DebugPrint("_xrgbToNv12.SetSize failed\n");
                return false;
            }
        }

        _isOpen = true;
        return true;
    }

    AVCodecContext* GetCtx() const
    {
        return _codecCtx;
    }

    EncodeInfo& Encode(AVFrame* frame, bool isEnd)
    {
        _encodeInfo.size = 0;
        if (!isEnd) {
            if (!_Trans(frame)) {
                __DebugPrint("_Trans failed\n");
                return _encodeInfo;
            }
            frame = _isHardware ? _ToHardware() : _bufferFrame;
            if (frame != nullptr) {
                frame->pts = _pts;
                ++_pts;
            }
        } else {
            frame = nullptr; // 直接刷新编码器缓存
        }

        if (avcodec_send_frame(_codecCtx, frame) < 0) {
            __DebugPrint("avcodec_send_frame failed\n");
            return _encodeInfo;
        }

        while (true) {
            while (_encodeInfo.data.size() <= _encodeInfo.size) {
                _encodeInfo.data.emplace_back();
            }
            auto&& packet = _encodeInfo.data[_encodeInfo.size].packet;
            int ret = avcodec_receive_packet(_codecCtx, packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return _encodeInfo;
            } else if (ret < 0) {
                __DebugPrint("avcodec_receive_packet : Error during encoding\n");
                return _encodeInfo;
            }
            ++_encodeInfo.size;
        }
    }

    void Close()
    {
        if (_codecCtx != nullptr) {
            avcodec_close(_codecCtx);
        }

        Free(_codecCtx, [this] { avcodec_free_context(&_codecCtx); });
        Free(_hwDeviceCtx, [this] { av_buffer_unref(&_hwDeviceCtx); });
        Free(_hwFrame, [this] { av_frame_free(&_hwFrame); });
    }

    static std::vector<std::string> FindEncoders()
    {
        std::vector<std::string> ret;
        for (const auto& name : _encoderNames) {
            if (avcodec_find_encoder_by_name(name) != nullptr) {
                ret.push_back(name);
            }
        }

        return ret;
    }

private:
    bool _Init(const EncodeParam& encodeParam)
    {
        _isHardware = encodeParam.name != "libx264";
        _pix_fmt = _isHardware ? AV_PIX_FMT_CUDA : AV_PIX_FMT_YUV420P;
        if (_isHardware && av_hwdevice_ctx_create(&_hwDeviceCtx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0) < 0) { // 硬件解码
            __DebugPrint("av_hwdevice_ctx_create failed\n");
            return false;
        }
        _codec = avcodec_find_encoder_by_name(encodeParam.name.c_str());
        if (!_codec) {
            __DebugPrint("avcodec_find_encoder failed\n");
            return false;
        }

        _codecCtx = avcodec_alloc_context3(_codec);
        if (_codecCtx == nullptr) {
            __DebugPrint("avcodec_alloc_context3 encoder failed\n");
            return false;
        }

        _codecCtx->bit_rate = encodeParam.bitRate;
        _codecCtx->width = encodeParam.width;
        _codecCtx->height = encodeParam.height;
        _codecCtx->time_base = {1, encodeParam.fps};
        _codecCtx->framerate = {encodeParam.fps, 1};

        // 影响缓冲区大小
        _codecCtx->gop_size = 10;
        _codecCtx->max_b_frames = 1;
        _codecCtx->pix_fmt = _pix_fmt;

        /* Some formats want stream headers to be separate. */
        if (encodeParam.fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            _codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        if (_isHardware && !_SetHwFrameCtx()) {
            __DebugPrint("_SetHwFrameCtx failed\n");
            return false;
        }

        _hwFrame = av_frame_alloc();
        if (_hwFrame == nullptr) {
            __DebugPrint("av_frame_alloc failed\n");
            return false;
        }

        return true;
    }
    bool _SetHwFrameCtx()
    {
        AVBufferRef* hwFramesRef;
        AVHWFramesContext* framesCtx = nullptr;

        if (!(hwFramesRef = av_hwframe_ctx_alloc(_hwDeviceCtx))) {
            __DebugPrint("av_hwframe_ctx_alloc failed\n");
            return false;
        }
        framesCtx = (AVHWFramesContext*)(hwFramesRef->data);
        framesCtx->format = AV_PIX_FMT_CUDA;
        framesCtx->sw_format = AV_PIX_FMT_NV12;
        framesCtx->width = _codecCtx->width;
        framesCtx->height = _codecCtx->height;
        framesCtx->initial_pool_size = 20;
        if (av_hwframe_ctx_init(hwFramesRef) < 0) {
            __DebugPrint("av_hwframe_ctx_init failed\n");
            av_buffer_unref(&hwFramesRef);
            return false;
        }
        _codecCtx->hw_frames_ctx = av_buffer_ref(hwFramesRef);
        if (!_codecCtx->hw_frames_ctx) {
            __DebugPrint("av_buffer_ref failed\n");
            return false;
        }

        av_buffer_unref(&hwFramesRef);
        return true;
    }

    bool _Trans(AVFrame* frame)
    {
        if (!_isOpen) {
            return false;
        }
        if (frame != nullptr) {
            if (!_isHardware) {
                _bufferFrame = frame->format == AV_PIX_FMT_BGR24 ? _rgbToYuv420.Trans(frame) : _xrgbToYuv420.Trans(frame);
            } else {
                _bufferFrame = frame->format == AV_PIX_FMT_BGR24 ? _rgbToNv12.Trans(frame) : _xrgbToNv12.Trans(frame);
            }

            if (_bufferFrame == nullptr) {
                __DebugPrint("Trans failed\n");
                return false;
            }
        }
        return true;
    }

    AVFrame* _ToHardware()
    {
        if (_bufferFrame == nullptr) {
            return nullptr;
        }

        if (av_hwframe_get_buffer(_codecCtx->hw_frames_ctx, _hwFrame, 0) < 0) {
            __DebugPrint("av_hwframe_get_buffer failed\n");
            return nullptr;
        }

        if (_hwFrame->hw_frames_ctx == nullptr) {
            __DebugPrint("_hardwareFrame->hw_frames_ctx is nullptr\n");
            return nullptr;
        }

        if (av_hwframe_transfer_data(_hwFrame, _bufferFrame, 0) < 0) {
            __DebugPrint("av_hwframe_transfer_data failed\n");
            return nullptr;
        }
        return _hwFrame;
    }

    bool _isOpen = false;
    bool _isHardware = false;
    EncodeInfo _encodeInfo;
    const AVCodec* _codec = nullptr;
    AVCodecContext* _codecCtx = nullptr;
    AVBufferRef* _hwDeviceCtx = nullptr;
    static constexpr const char* _encoderNames[4] = {
        "h264_nvenc",
        "h264_qsv",
        "h264_amf",
        "libx264",
    };
    PixTransformer<AV_PIX_FMT_BGR24, AV_PIX_FMT_YUV420P> _rgbToYuv420;
    PixTransformer<AV_PIX_FMT_BGR0, AV_PIX_FMT_YUV420P> _xrgbToYuv420;
    PixTransformer<AV_PIX_FMT_BGR24, AV_PIX_FMT_NV12> _rgbToNv12;
    PixTransformer<AV_PIX_FMT_BGR0, AV_PIX_FMT_NV12> _xrgbToNv12;
    AVFrame* _bufferFrame = nullptr;
    AVFrame* _hwFrame = nullptr;
    uint64_t _pts = 0;
    AVPixelFormat _pix_fmt = AV_PIX_FMT_YUV420P;
};