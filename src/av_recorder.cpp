/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2020-02-03 10:00:08
 * @Description:
 */
#include "av_recorder.h"
#include "av_capturer.h"
#include "av_muxer.h"
#include "video_render.h"
#include "video_widget.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>

AvRecorder::AvRecorder(QWidget* parent)
    : QMainWindow(parent)
{
    _capturer = new AvCapturer;
    _muxer = new AvMuxer;
    _videoRender = new VideoRender;
    _isRecord = false;
    _InitUi();
    _StartCapture();
    _StartPreview();
    _InitConnect();
}

AvRecorder::~AvRecorder()
{
    _StopRecord();
    _StopPreview();
    _StopCapture();
    Free(_muxer, [this] { delete _muxer; });
    Free(_videoRender, [this] { delete _videoRender; });
}

void AvRecorder::_StartCapture()
{
    // if (!_capturer->Open(FindWindow(nullptr, TEXT("Plants vs. Zombies")))) {
    if (!_capturer->Open(GetDesktopWindow())) {
        __DebugPrint("_capturer->Open failed\n");
        return;
    }

    {
        std::lock_guard<std::mutex> lk(_renderMtx);
        auto fmt = _capturer->GetIsUseDxgi() ? AV_PIX_FMT_BGR0 : AV_PIX_FMT_BGR24;
        _renderFrame = AllocFrame(fmt, _capturer->GetWidth(), _capturer->GetHeight());
    }

    _captureTimer.Start(1000 / _fps, [this] {
        auto srcFrame = _capturer->GetFrame();
        if (srcFrame != nullptr && windowState() != Qt::WindowMinimized) {
            std::unique_lock<std::mutex> renderLk(_renderMtx);
            if (av_frame_copy(_renderFrame, srcFrame) < 0) {
                __DebugPrint("av_frame_copy failed\n");
                return;
            }
            _renderCondVar.notify_all();
        }
        if (_isRecord) {
            std::lock_guard<std::mutex> muxLk(_muxMtx);
            AvFrame dstFrame(srcFrame);
            _mtxQueue.Push(std::move(dstFrame));
        }
        _muxCondVar.notify_all();
    });
}

void AvRecorder::_StopCapture()
{
    _captureTimer.Stop();
    Free(_capturer, [this] { _capturer->Close(); delete _capturer; });
    Free(_renderFrame, [this] { av_frame_free(&_renderFrame); });
}

void AvRecorder::_StartPreview()
{
    int width = _capturer->GetWidth();
    int height = _capturer->GetHeight();
    if (!_videoRender->Open(_videoWidget->GetHwnd(), width, height)) {
        __DebugPrint("_videoRender->Init failed\n");
        return;
    }
    _videoWidget->SetScaleFixSize(width, height);

    _renderTimer.Start(0, [this] {
        std::unique_lock<std::mutex> lk(_renderMtx);
        _renderCondVar.wait(lk);
        if (!_videoRender->Trans(_renderFrame)) {
            __DebugPrint("_videoRender->Trans() failed\n");
            return;
        }
        lk.unlock();
        if (!_videoRender->Render()) {
            __DebugPrint("_videoRender->Render() failed\n");
            return;
        }
    });
}

void AvRecorder::_StopPreview()
{
    _renderTimer.Stop();
    _videoRender->Close();
}

void AvRecorder::_StartRecord()
{
    if (!_muxer->Open("_test.mp4", _capturer->GetWidth(), _capturer->GetHeight(), _fps)) {
        __DebugPrint("_muxer->Open failed\n");
        return;
    }
    _muxTimer.Start(0, [this] {
        std::unique_lock<std::mutex> lk(_muxMtx);
        if (_mtxQueue.IsEmpty()) {
            _muxCondVar.wait(lk);
        }
        if (_mtxQueue.IsEmpty()) { // 说明录制结束了
            return;
        }
        auto frame = _mtxQueue.Pop();
        lk.unlock();
        if (!_muxer->Write(frame.frame)) {
            __DebugPrint("_muxer->Write failed\n");
            return;
        }
    });
    _isRecord = true;
}

void AvRecorder::_StopRecord()
{
    _isRecord = false;
    _muxTimer.Stop();
    _muxer->Close();
}

void AvRecorder::_InitConnect()
{
    connect(_recordBtn, &QPushButton::released, [this] {
        __DebugPrint("Release _recordBtn %d\n", _isRecord);
        if (!_isRecord) {
            _StartRecord();
            _recordBtn->setText("停止录制");
        } else {
            _StopRecord();
            _recordBtn->setText("开始录制");
        }
    });
}

void AvRecorder::_InitUi()
{
    setFont(QFont("Microsoft Yahei"));
    _videoWidget = new VideoWidget;
    resize(800, 600);
    _recordBtn = new QPushButton("开始录制");
    auto widget = new QWidget;
    auto layout = new QVBoxLayout;
    layout->addWidget(_videoWidget);
    layout->addWidget(_recordBtn);
    widget->setLayout(layout);
    this->setCentralWidget(widget);
}
