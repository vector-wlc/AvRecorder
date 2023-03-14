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
    __CheckBool(_encodeFrame = Frame<MediaType::VIDEO>::Alloc(
                    AV_PIX_FMT_NV12, _capturer.GetWidth(), _capturer.GetHeight()));
    {
        std::lock_guard<std::mutex> renderLk(_renderMtx);
        __CheckBool(_renderFrame = Frame<MediaType::VIDEO>::Alloc(
                        AV_PIX_FMT_NV12, _capturer.GetWidth(), _capturer.GetHeight()));
    }

    // 开始捕获画面
    _captureTimer.Start(param.fps, [this] {
        auto srcFrame = _capturer.GetFrame();
        if (srcFrame != nullptr) {
            std::lock_guard<std::mutex> muxLk(__mtx);
            if (srcFrame->format != _encodeFrame->format) {
                std::lock_guard<std::mutex> renderLk(_renderMtx);
                Free(_encodeFrame, [this] { av_frame_free(&_encodeFrame); });
                __CheckNo(_encodeFrame = Frame<MediaType::VIDEO>::Alloc(
                              AVPixelFormat(srcFrame->format), _capturer.GetWidth(), _capturer.GetHeight()));
            }
            av_frame_copy(_encodeFrame, srcFrame);
        }
    });
    param.width = _capturer.GetWidth();
    param.height = _capturer.GetHeight();
    _param = param;
    return true;
}

AVFrame* VideoRecorder::GetRenderFrame()
{
    std::lock_guard<std::mutex> renderLk(_renderMtx);
    if (_renderFrame->format != _encodeFrame->format) {
        Free(_renderFrame, [this] { av_frame_free(&_renderFrame); });
        __CheckNullptr(_renderFrame = Frame<MediaType::VIDEO>::Alloc(
                           AVPixelFormat(_encodeFrame->format), _capturer.GetWidth(), _capturer.GetHeight()));
    }
    av_frame_copy(_renderFrame, _encodeFrame);
    return _renderFrame;
}

bool VideoRecorder::LoadMuxer(AvMuxer& muxer)
{
    _muxer = &muxer;
    __CheckBool((_streamIndex = muxer.AddVideoStream(_param)) != -1);
    return true;
}

bool VideoRecorder::StartRecord()
{
    _totalPts = 0;
    _lossPts = 0;
    _muxTimer.Start(_param.fps, [this] {
        ++_totalPts;
        if (!_muxer->Write(_encodeFrame, _streamIndex)) {
            ++_lossPts;
        }
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
    Free(_renderFrame, [this] { av_frame_free(&_renderFrame); });
}