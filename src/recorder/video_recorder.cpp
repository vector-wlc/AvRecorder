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
    _cnt = 0;
    __CheckBool(_encodeFrame = Frame<MediaType::VIDEO>::Alloc(
                    AV_PIX_FMT_BGR0, _capturer.GetWidth(), _capturer.GetHeight()));

    // 开始捕获画面
    _captureTimer.Start(param.fps, [this] {
        auto srcFrame = _capturer.GetFrame();
        if (srcFrame != nullptr) {
            std::lock_guard<std::mutex> muxLk(__mtx);
            if (srcFrame->format != _encodeFrame->format) {
                _cnt = 0;
                // 由于渲染那边在不同的线程
                // 这里的 Free 可能直接使得 _encodeFrame 的内存失效
                // 导致崩溃
                // 所以搞一个计数器, 当当前的有效帧数目大于 5 时, 捕获就非常稳定了
                // 就可以直接让 GetRenderFrame 返回了
                Free(_encodeFrame, [this] { av_frame_free(&_encodeFrame); });
                __CheckNo(_encodeFrame = Frame<MediaType::VIDEO>::Alloc(
                              AVPixelFormat(srcFrame->format), _capturer.GetWidth(), _capturer.GetHeight()));
            }
            av_frame_copy(_encodeFrame, srcFrame);
            ++_cnt;
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
    _cnt = 0;
    StopRecord();
    _captureTimer.Stop();
    _capturer.Close();
    Free(_encodeFrame, [this] { av_frame_free(&_encodeFrame); });
}