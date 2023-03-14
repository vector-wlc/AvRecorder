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
    AVFrame* GetRenderFrame();
    // 停止录制
    void Close();
    void SetIsDrawCursor(bool isDraw)
    {
        _capturer.SetDrawCursor(isDraw);
    }
    bool IsCaptureOverload() const { return _captureTimer.IsOverload(); }
    double GetLossRate() { return _lossPts == 0 ? 0 : (double)_lossPts / _totalPts; }

private:
    bool _Open(Encoder<MediaType::VIDEO>::Param& param);
    VideoCapturer _capturer;
    AvMuxer* _muxer = nullptr;
    bool _isRecord = false;
    int _streamIndex = -1;
    AVFrame* _encodeFrame = nullptr;
    AVFrame* _renderFrame = nullptr;
    Encoder<MediaType::VIDEO>::Param _param;
    Timer _captureTimer;
    Timer _muxTimer;
    std::mutex _renderMtx;
    uint64_t _totalPts = 0;
    uint64_t _lossPts = 0;
};
#endif