/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-03 10:00:08
 * @Description:
 */
#pragma once

#include "audio_widget.h"
#include "recorder/audio_recorder.h"
#include "recorder/video_recorder.h"
#include "ui/settings_page.h"
#include "video_render.h"
#include "video_widget.h"
#include <QCheckBox>
#include <QLayout>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QTime>
#include <QTimer>
#include <condition_variable>
#include <queue>

class AvRecorder : public QMainWindow {
    Q_OBJECT

public:
    AvRecorder(QWidget* parent = nullptr);
    ~AvRecorder();

private:
    AudioRecorder _audioRecorder;
    VideoRecorder _videoRecorder;
    AvMuxer _avMuxer;
    VideoRender _videoRender;
    VideoWidget* _videoWidget = nullptr;
    AudioWidget* _microphoneWidget = nullptr;
    AudioWidget* _speakerWidget = nullptr;
    QPushButton* _recordBtn = nullptr;
    QPushButton* _settingsBtn = nullptr;
    QCheckBox* _isDrawCursorBox = nullptr;
    Timer _videoRenderTimer;
    QTimer _otherTimer;
    QListWidget* _captureListWidget = nullptr;
    QPushButton* _updateListBtn = nullptr;
    bool _isRecord = false;
    void _InitUi();
    QComboBox* _captureMethodBox = nullptr;
    QLabel* _captureStatusLabel = nullptr;
    QLabel* _captureTimeLabel = nullptr;
    QLabel* _fpsLabel = nullptr;
    QLabel* _videoEncodeLabel = nullptr;
    SettingsPage::Param _settingsParam;
    QVBoxLayout* _InitListUi();
    QVBoxLayout* _InitAudioUi();
    QVBoxLayout* _InitOtherUi();
    QTime _recordTime;
    bool _isLocked = false;
    void _InitStatusBarUi();
    void _UpdateCaptureList();
    void _StartCapture(VideoCapturer::Method method);
    void _StopCapture();
    void _StartPreview();
    void _DealCapture();
    void _StopPreview();
    void _StartRecord();
    void _StopRecord();
    void _InitConnect();
};