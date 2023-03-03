/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-19 15:41:37
 * @Description:
 */
#ifndef __VIDEO_ENCODER_H__
#define __VIDEO_ENCODER_H__

#include "abstract_encoder.h"
#include "basic/frame.h"

template <>
class Encoder<MediaType::VIDEO> : public AbstractEncoder {
public:
    struct Param {
        int bitRate;
        int width;
        int height;
        int fps;
        std::string name;
    };
    Encoder();
    ~Encoder() { Close(); }
    bool Open(const Param& encodeParam, AVFormatContext* fmtCtx);
    virtual bool PushFrame(AVFrame* frame, bool isEnd, uint64_t pts) override;
    virtual void AfterEncode() override;
    virtual void Close() override;
    static const std::vector<std::string>& GetUsableEncoders();

private:
    bool
    _Init(const Param& encodeParam, AVFormatContext* fmtCtx);
    bool _SetHwFrameCtx();
    bool _Trans(AVFrame* frame);
    AVFrame* _ToHardware();
    static void _FindUsableEncoders();
    bool _isHardware = false;
    PixTransformer _rgbToNv12;
    PixTransformer _xrgbToNv12;
    AVFrame* _bufferFrame = nullptr;
    static constexpr const char* _encoderNames[4] = {
        "h264_nvenc",
        "h264_qsv",
        "h264_amf",
        "libx264",
    };
    static std::vector<std::string> _usableEncoders;
    AVBufferRef* _hwDeviceCtx = nullptr;
    AVFrame* _hwFrame = nullptr;
    AVPixelFormat _pixFmt = AV_PIX_FMT_NV12;
};

#endif