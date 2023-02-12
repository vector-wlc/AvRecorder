/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-03 10:00:08
 * @Description:
 */
#pragma once

#include "mutex_queue.h"
#include "timer.h"
#include <QMainWindow>
#include <condition_variable>

class AvCapturer;
class AvMuxer;
class VideoRender;
class VideoWidget;
class QPaintEvent;
class QTimer;
class QPushButton;

class AvRecorder : public QMainWindow {
    Q_OBJECT

public:
    AvRecorder(QWidget* parent = nullptr);
    ~AvRecorder();

private:
    AvCapturer* _capturer;
    AvMuxer* _muxer;
    VideoRender* _videoRender;
    VideoWidget* _videoWidget;
    QPushButton* _recordBtn;
    AVFrame* _renderFrame = nullptr;
    bool _isRecord = false;
    Timer _captureTimer;
    Timer _renderTimer;
    Timer _muxTimer;
    std::condition_variable _muxCondVar;
    std::mutex _muxMtx;
    std::condition_variable _renderCondVar;
    std::mutex _renderMtx;
    MutexQueue<AvFrame> _mtxQueue;
    int _fps = 30;

    void _InitUi();
    void _StartCapture();
    void _StopCapture();
    void _StartPreview();
    void _StopPreview();
    void _StartRecord();
    void _StopRecord();
    void _InitConnect();
};