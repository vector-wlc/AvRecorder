/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-20 10:33:43
 * @Description:
 */
#ifndef __VIDEO_RECORDER_H__
#define __VIDEO_RECORDER_H__

#include "basic/timer.h"
#include "capturer/video_capturer.h"
#include "muxer/av_muxer.h"
#include <condition_variable>
#include <queue>

class VideoRecorder {
public:
    bool Open(HWND srcHwnd, Encoder<MediaType::VIDEO>::Param& param);
    bool LoadMuxer(AvMuxer& muxer);
    bool StartRecord();
    void StopRecord();
    bool IsUseDxgi() { return _capturer.IsUseDxgi(); }
    AVFrame* GetRenderFrame() const { return _renderFrame; };
    // 停止录制
    void Close();
    void SetIsDrawCursor(bool isDraw)
    {
        _isDrawCursor = isDraw;
    }
    bool IsEncodeOverload() const { return _isEncodeOverload; }
    bool IsCaptureOverload() const { return _captureTimer.IsOverload(); }

private:
    VideoCapturer _capturer;
    AvMuxer* _muxer = nullptr;
    bool _isRecord = false;
    int _streamIndex = -1;
    AVFrame* _renderFrame = nullptr;
    std::condition_variable _muxCondVar;
    std::mutex _muxMtx;
    Encoder<MediaType::VIDEO>::Param _param;
    std::queue<Frame<MediaType::VIDEO>> _queue;
    Timer _captureTimer;
    Timer _muxTimer;
    bool _isDrawCursor = true;
    bool _isEncodeOverload = false;
};
#endif