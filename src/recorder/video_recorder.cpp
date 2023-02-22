/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-20 10:59:34
 * @Description:
 */
#include "video_recorder.h"

bool VideoRecorder::Open(HWND srcHwnd, Encoder<MediaType::VIDEO>::Param& param)
{
    Close();
    _isEncodeOverload = false;
    __CheckBool(_capturer.Open(srcHwnd));
    auto fmt = _capturer.IsUseDxgi() ? AV_PIX_FMT_BGR0 : AV_PIX_FMT_BGR24;
    _renderFrame = Frame<MediaType::VIDEO>::Alloc(fmt, _capturer.GetWidth(), _capturer.GetHeight());

    // 开始捕获画面
    _captureTimer.Start(param.fps, [this] {
        auto srcFrame = _capturer.GetFrame(_isDrawCursor);
        if (srcFrame != nullptr) {
            av_frame_copy(_renderFrame, srcFrame);
        }
        if (_isRecord) {
            std::lock_guard<std::mutex> muxLk(_muxMtx);
            _queue.emplace(_renderFrame);
        }
        _muxCondVar.notify_all();
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
    _muxTimer.Start(0, [this] {
        std::unique_lock<std::mutex> lk(_muxMtx);
        if (_queue.empty()) {
            _muxCondVar.wait(lk);
        }
        if (_queue.empty()) { // 说明录制结束了
            return;
        }
        std::queue<Frame<MediaType::VIDEO>> tmpQueue;
        tmpQueue.swap(_queue);
        lk.unlock();
        _isEncodeOverload = tmpQueue.size() > float(_param.fps) / 3;
        if (tmpQueue.size() > float(_param.fps)) {
            return; // 直接弃用这些帧
        }
        while (!tmpQueue.empty()) {
            auto frame = std::move(tmpQueue.front());
            tmpQueue.pop();
            __CheckNo(_muxer->Write(frame.frame, _streamIndex));
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
}