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
    bool Open(HWND srcHwnd, Encoder<MediaType::VIDEO>::Param& param, VideoCapturer::Method type);
    bool Open(int monitorIdx, Encoder<MediaType::VIDEO>::Param& param, VideoCapturer::Method type);
    bool LoadMuxer(AvMuxer& muxer);
    bool StartRecord();
    void StopRecord();
    auto GetCapturerType() { return _capturer.GetMethod(); }
    AVFrame* GetRenderFrame() const { return _encodeFrame; };
    // 停止录制
    void Close();
    void SetIsDrawCursor(bool isDraw)
    {
        _capturer.SetDrawCursor(isDraw);
    }
    bool IsEncodeOverload() const { return _isEncodeOverload; }
    bool IsCaptureOverload() const { return _captureTimer.IsOverload(); }

private:
    bool _Open(Encoder<MediaType::VIDEO>::Param& param);
    VideoCapturer _capturer;
    AvMuxer* _muxer = nullptr;
    bool _isRecord = false;
    int _streamIndex = -1;
    AVFrame* _encodeFrame = nullptr;
    Encoder<MediaType::VIDEO>::Param _param;
    Timer _captureTimer;
    Timer _muxTimer;
    bool _isEncodeOverload = false;
};
#endif