

/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-03 10:01:33
 * @Description:
 */
#include "video_encoder.h"

extern "C" {
#include <libavutil/opt.h>
}

std::vector<std::string> Encoder<MediaType::VIDEO>::_usableEncoders;

Encoder<MediaType::VIDEO>::Encoder()
    : _rgbToNv12(AV_PIX_FMT_BGR24, AV_PIX_FMT_NV12)
    , _xrgbToNv12(AV_PIX_FMT_BGR0, AV_PIX_FMT_NV12)
{
}

bool Encoder<MediaType::VIDEO>::Open(const Param& encodeParam, AVFormatContext* fmtCtx)
{
    Close();
    _isOpen = false;
    __CheckBool(_Init(encodeParam, fmtCtx));

    // 打开编码器
    __CheckBool(avcodec_open2(_codecCtx, _codec, nullptr) >= 0);

    __CheckBool(_rgbToNv12.SetSize(encodeParam.width, encodeParam.height));
    __CheckBool(_xrgbToNv12.SetSize(encodeParam.width, encodeParam.height));

    _isOpen = true;
    return true;
}

bool Encoder<MediaType::VIDEO>::PushFrame(AVFrame* frame, bool isEnd, uint64_t pts)
{
    if (!isEnd) {
        __CheckBool(_Trans(frame));
        frame = _bufferFrame;
        __CheckBool(frame);
    } else {
        frame = nullptr; // 直接刷新编码器缓存
    }
    if (frame != nullptr) {
        frame->pts = pts;
    }
    __CheckBool(avcodec_send_frame(_codecCtx, frame) >= 0);
    return true;
}

void Encoder<MediaType::VIDEO>::AfterEncode()
{
    if (_isHardware) {
        Free(_hwFrame, [this] { av_frame_free(&_hwFrame); });
    }
}

void Encoder<MediaType::VIDEO>::Close()
{
    if (_codecCtx != nullptr) {
        avcodec_close(_codecCtx);
    }

    Free(_codecCtx, [this] { avcodec_free_context(&_codecCtx); });
    Free(_hwDeviceCtx, [this] { av_buffer_unref(&_hwDeviceCtx); });
}

const std::vector<std::string>& Encoder<MediaType::VIDEO>::GetUsableEncoders()
{
    if (_usableEncoders.empty()) {
        _FindUsableEncoders();
    }
    return _usableEncoders;
}

void Encoder<MediaType::VIDEO>::_FindUsableEncoders()
{
    // 尝试打开编码器看看编码器能不能用
    Param param;
    param.bitRate = 1000;
    param.fps = 30;
    param.width = 1920;
    param.height = 1080;
    Encoder encoder;
    AVFormatContext* fmtCtx = nullptr;

    __CheckNo(avformat_alloc_output_context2(&fmtCtx, nullptr, nullptr, "test.mp4") >= 0);
    for (const auto& name : _encoderNames) {
        if (strcmp(name, "libx264") == 0) { // 软件编码器必定支持
            _usableEncoders.push_back(name);
            continue;
        }
        param.name = name;
        if (encoder.Open(param, fmtCtx)) {
            _usableEncoders.push_back(name);
        }
        encoder.Close();
    }
    Free(fmtCtx, [&fmtCtx] { avformat_free_context(fmtCtx); });
}

bool Encoder<MediaType::VIDEO>::_Init(const Param& encodeParam, AVFormatContext* fmtCtx)
{
    _isHardware = encodeParam.name != "libx264";
    _pixFmt = _isHardware ? AV_PIX_FMT_CUDA : AV_PIX_FMT_NV12;
    if (_isHardware && av_hwdevice_ctx_create(&_hwDeviceCtx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0) < 0) { // 硬件解码
        __DebugPrint("av_hwdevice_ctx_create failed\n");
        return false;
    }
    __CheckBool(_codec = avcodec_find_encoder_by_name(encodeParam.name.c_str()));
    __CheckBool(_codecCtx = avcodec_alloc_context3(_codec));
    _codecCtx->bit_rate = encodeParam.bitRate;
    _codecCtx->width = encodeParam.width;
    _codecCtx->height = encodeParam.height;
    _codecCtx->time_base = {1, encodeParam.fps};
    _codecCtx->framerate = {encodeParam.fps, 1};

    // 影响缓冲区大小
    _codecCtx->gop_size = 10;
    _codecCtx->max_b_frames = 1;
    _codecCtx->pix_fmt = _pixFmt;

    /* Some formats want stream headers to be separate. */
    if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        _codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (!_isHardware) { // 软件编码设置为快，避免占用过高的 CPU ，反正硬盘不值钱
        av_opt_set(_codecCtx->priv_data, "preset", "veryfast", 0);
    }

    __CheckBool(!_isHardware || _SetHwFrameCtx());
    return true;
}
bool Encoder<MediaType::VIDEO>::_SetHwFrameCtx()
{
    AVBufferRef* hwFramesRef;
    AVHWFramesContext* framesCtx = nullptr;

    __CheckBool(hwFramesRef = av_hwframe_ctx_alloc(_hwDeviceCtx));
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
    __CheckBool(_codecCtx->hw_frames_ctx = av_buffer_ref(hwFramesRef));
    av_buffer_unref(&hwFramesRef);
    return true;
}

bool Encoder<MediaType::VIDEO>::_Trans(AVFrame* frame)
{
    std::lock_guard<std::mutex> lk(__mtx);
    if (!_isOpen) {
        return false;
    }
    switch (frame->format) {
    case AV_PIX_FMT_BGR24:
        _bufferFrame = _rgbToNv12.Trans(frame);
        break;

    case AV_PIX_FMT_BGR0:
        _bufferFrame = _rgbToNv12.Trans(frame);
        break;

    default: // NV12
        _bufferFrame = frame;
        break;
    }
    if (_isHardware) {
        _bufferFrame = _ToHardware();
    }
    __CheckBool(_bufferFrame);
    return true;
}

AVFrame* Encoder<MediaType::VIDEO>::_ToHardware()
{
    if (_bufferFrame == nullptr) {
        return nullptr;
    }
    __CheckNullptr(_hwFrame = av_frame_alloc());
    __CheckNullptr(av_hwframe_get_buffer(_codecCtx->hw_frames_ctx, _hwFrame, 0) >= 0);
    __CheckNullptr(_hwFrame->hw_frames_ctx);
    __CheckNullptr(av_hwframe_transfer_data(_hwFrame, _bufferFrame, 0) >= 0);
    return _hwFrame;
}
