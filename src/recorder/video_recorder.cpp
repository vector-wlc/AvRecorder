/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-20 10:59:34
 * @Description:
 */
#include "video_recorder.h"

bool VideoRecorder::Open(HWND srcHwnd, Encoder<MediaType::VIDEO>::Param& param, VideoCapturer::Method type)
{
    Close();
    __CheckBool(_capturer.Open(srcHwnd, type));
    __CheckBool(_Open(param));
    return true;
}

bool VideoRecorder::Open(int monitorIdx, Encoder<MediaType::VIDEO>::Param& param, VideoCapturer::Method type)
{
    Close();
    __CheckBool(_capturer.Open(monitorIdx, type));
    __CheckBool(_Open(param));
    return true;
}

bool VideoRecorder::_Open(Encoder<MediaType::VIDEO>::Param& param)
{
    _isEncodeOverload = false;
    switch (_capturer.GetMethod()) {
    case VideoCapturer::WGC:
    case VideoCapturer::DXGI:
        _encodeFrame = Frame<MediaType::VIDEO>::Alloc(AV_PIX_FMT_BGR0, _capturer.GetWidth(), _capturer.GetHeight());
        break;
    default: // GDI
        _encodeFrame = Frame<MediaType::VIDEO>::Alloc(AV_PIX_FMT_BGR24, _capturer.GetWidth(), _capturer.GetHeight());
        break;
    }

    // 开始捕获画面
    _captureTimer.Start(param.fps, [this] {
        auto srcFrame = _capturer.GetFrame();
        if (srcFrame != nullptr) {
            std::lock_guard<std::mutex> muxLk(__mtx);
            av_frame_copy(_encodeFrame, srcFrame);
        }
    });
    param.width = _capturer.GetWidth();
    param.height = _capturer.GetHeight();
    _param = param;
    return true;
}

bool VideoRecorder::LoadMuxer(AvMuxer& muxer)
{
    _muxer = &muxer;
    __CheckBool((_streamIndex = muxer.AddVideoStream(_param)) != -1);
    return true;
}

bool VideoRecorder::StartRecord()
{
    _muxTimer.Start(_param.fps, [this] {
        __CheckNo(_muxer->Write(_encodeFrame, _streamIndex));
    });
    _isRecord = true;
    return true;
}
void VideoRecorder::StopRecord()
{
    _isRecord = false;
    _muxTimer.Stop();
}

void VideoRecorder::Close()
{
    StopRecord();
    _captureTimer.Stop();
    _capturer.Close();
    Free(_encodeFrame, [this] { av_frame_free(&_encodeFrame); });
}