/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2020-02-03 10:00:08
 * @Description:
 */

#include "av_recorder.h"
#include <QDateTime>
#include <QStatusBar>
#include <capturer/finder.h>

AvRecorder::AvRecorder(QWidget* parent)
    : QMainWindow(parent)
{
    _settingsParam.audioParam.bitRate = 160'000;
    _settingsParam.videoParam.bitRate = 8000'000;
    _settingsParam.videoParam.fps = 30;
    _settingsParam.videoParam.name = Encoder<MediaType::VIDEO>::GetUsableEncoders().front();
    _settingsParam.outputDir = ".";
    _settingsParam.liveUrl = "rtmp://127.0.0.1:1935";
    _settingsParam.liveName = "stream";

    WgcCapturer::Init();
    _InitUi();
    _InitConnect();
}

void AvRecorder::_InitConnect()
{
    // 启动
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [this, timer] {
        _isLocked = true;
        _StopPreview();
        _StopCapture();
        _StartCapture(VideoCapturer::WGC);
        _StartPreview();
        _isLocked = false;
        timer->stop();
    });
    timer->start(100);

    connect(_recordBtn, &QPushButton::released, [this] {
        if (!_isRecord) {
            auto fileName = _settingsParam.outputDir;
            if (fileName.back() != '\\') {
                fileName.push_back('\\');
            }
            auto format = "mp4";
            fileName += QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss").toStdString() + "." + format;
            // fileName += std::string("test.") + format;
            __CheckNo(_StartStream(fileName, format));
            _liveBtn->setEnabled(false);
            _recordBtn->setText("停止录制");
        } else {
            _StopStream();
            _liveBtn->setEnabled(true);
            _recordBtn->setText("开始录制");
        }
        _isRecord = !_isRecord;
    });
    connect(_liveBtn, &QPushButton::released, [this] {
        if (!_isLive) {
            auto fileName = _settingsParam.liveUrl + "/" + _settingsParam.liveName;
            bool isRtsp = _settingsParam.liveUrl.find("rtsp") != std::string::npos;
            __CheckNo(_StartStream(fileName, isRtsp ? "rtsp" : "flv"));
            _recordBtn->setEnabled(false);
            _liveBtn->setText("停止直播");
        } else {
            _StopStream();
            _recordBtn->setEnabled(true);
            _liveBtn->setText("开始直播");
        }
        _isLive = !_isLive;
    });
    connect(_microphoneWidget, &AudioWidget::SetVolumeScale, [this](float scale) {
        _audioRecorder.SetVolumeScale(scale, MICROPHONE_INDEX);
    });
    connect(_speakerWidget, &AudioWidget::SetVolumeScale, [this](float scale) {
        _audioRecorder.SetVolumeScale(scale, SPEAKER_INDEX);
    });
    connect(_updateListBtn, &QPushButton::released, [this] {
        _UpdateCaptureList();
    });
    connect(_captureListWidget, &QListWidget::currentTextChanged, [this](const QString& text) {
        if (text.isEmpty() || _isLocked) {
            return;
        }
        _isLocked = true;
        _StopPreview();
        _StopCapture();
        _StartCapture(VideoCapturer::WGC);
        _StartPreview();
        _isLocked = false;
    });
    connect(_isDrawCursorBox, &QCheckBox::stateChanged, [this] {
        _videoRecorder.SetIsDrawCursor(_isDrawCursorBox->isChecked());
    });
    connect(_captureMethodBox, &QComboBox::currentTextChanged, [this](const QString& text) {
        if (_isLocked || text.isEmpty()) {
            return;
        }
        _StopPreview();
        _StopCapture();
        if (text == "WGC") {
            _StartCapture(VideoCapturer::WGC);
        } else if (text == "DXGI") {
            _StartCapture(VideoCapturer::DXGI);
        } else {
            _StartCapture(VideoCapturer::GDI);
        }
        _StartPreview();
    });
    connect(_settingsBtn, &QPushButton::released, [this] {
        auto settingsPage = std::make_unique<SettingsPage>(&_settingsParam, this);
        settingsPage->exec();
        _isLocked = true;
        _StopPreview();
        _StopCapture();
        _StartCapture(VideoCapturer::WGC);
        _StartPreview();
        _isLocked = false;
    });

    _otherTimer.callOnTimeout([this] {
        if (windowState() == Qt::WindowMinimized) {
            return;
        }
        // 音频
        auto info = _audioRecorder.GetCaptureInfo(MICROPHONE_INDEX);
        _microphoneWidget->ShowVolume(info == nullptr ? 0 : info->volume);
        info = _audioRecorder.GetCaptureInfo(SPEAKER_INDEX);
        _speakerWidget->ShowVolume(info == nullptr ? 0 : info->volume);
        // 状态栏
        if (_isRecord || _isLive) {
            int interval = _recordTime.secsTo(QTime::currentTime());
            int sec = interval % 60;
            interval /= 60;
            int minute = interval % 60;
            int hour = interval / 60;
            _captureTimeLabel->setText(
                QString("%1:%2:%3").arg(hour, 2, 10, QChar('0')).arg(minute, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0')));
            auto lossRate = _videoRecorder.GetLossRate();
            int num = lossRate * 10000;
            _videolossRate->setText(QString("丢帧率: %1.%2%").arg(num / 100, 2, 10, QChar('0')).arg(num % 100, 2, 10, QChar('0')));
        } else if (_captureTimeLabel->text() != "00:00:00") {
            _captureTimeLabel->setText("00:00:00");
        }
    });
}

AvRecorder::~AvRecorder()
{
    _StopStream();
    _StopPreview();
    _StopCapture();
    WgcCapturer::Uninit();
}

void AvRecorder::_StartCapture(VideoCapturer::Method method)
{
    if (_isLocked) {
        _captureMethodBox->clear();
        _captureMethodBox->addItem("WGC");
    }

    // 判断是要捕获屏幕还是窗口
    int idx = _captureListWidget->currentRow();
    if (idx < 0) {
        idx = 0;
        _captureListWidget->setCurrentRow(idx);
    }

    int monitorCnt = MonitorFinder::GetList().size();
    if (idx < monitorCnt) { // 捕获屏幕
        if (_captureMethodBox->count() < 2) {
            _captureMethodBox->addItem("DXGI");
        }

        _videoRecorder.Open(idx, _settingsParam.videoParam, method);

    } else {
        if (_captureMethodBox->count() < 2) {
            _captureMethodBox->addItem("GDI");
        }
        auto hwnd = WindowFinder::GetList()[idx - monitorCnt].hwnd;

        _videoRecorder.Open(hwnd, _settingsParam.videoParam, method);
    }
    _DealCapture();
    _isDrawCursorBox->setEnabled(true);
    _recordBtn->setEnabled(true);
    _liveBtn->setEnabled(true);
    _videoRecorder.SetIsDrawCursor(_isDrawCursorBox->isChecked());
    _audioRecorder.SetVolumeScale(_microphoneWidget->GetVolume(), MICROPHONE_INDEX);
    _audioRecorder.SetVolumeScale(_speakerWidget->GetVolume(), SPEAKER_INDEX);
}

void AvRecorder::_DealCapture()
{
    __CheckNo(_audioRecorder.Open({AudioCapturer::Microphone, AudioCapturer::Speaker}, _settingsParam.audioParam));
    _microphoneWidget->setEnabled(_audioRecorder.GetCaptureInfo(MICROPHONE_INDEX) != nullptr);
    _speakerWidget->setEnabled(_audioRecorder.GetCaptureInfo(SPEAKER_INDEX) != nullptr);
    _fpsLabel->setText(QString("FPS: %1").arg(_settingsParam.videoParam.fps));
    _videoEncodeLabel->setText(("编码器: " + _settingsParam.videoParam.name).c_str());
}

void AvRecorder::_StopCapture()
{
    _videoRecorder.Close();
    _audioRecorder.Close();
}

void AvRecorder::_StartPreview()
{
    __CheckNo(_videoRender.Open(_videoWidget->GetHwnd(), _settingsParam.videoParam.width, _settingsParam.videoParam.height));
    _videoWidget->SetScaleFixSize(_settingsParam.videoParam.width, _settingsParam.videoParam.height);

    // 视频需要做到和帧率一样的渲染速度，QTimer 达不到要求
    // 需要自己封装一个计时器
    _videoRenderTimer.Start(_settingsParam.videoParam.fps, [this] {
        if (windowState() == Qt::WindowMinimized) {
            return;
        }
        // 视频
        auto frame = _videoRecorder.GetRenderFrame();
        __CheckNo(_videoRender.Render(frame));
    });

    // 刷新率设置为 25
    _otherTimer.start(40);
}

void AvRecorder::_StopPreview()
{
    _videoRenderTimer.Stop();
    _videoRender.Close();
    _otherTimer.stop();
}

bool AvRecorder::_StartStream(std::string_view path, std::string_view format)
{
    __CheckBool(_avMuxer.Open(path, format));
    __CheckBool(_audioRecorder.LoadMuxer(_avMuxer));
    __CheckBool(_videoRecorder.LoadMuxer(_avMuxer));
    __CheckBool(_avMuxer.WriteHeader());
    __CheckBool(_audioRecorder.StartRecord());
    __CheckBool(_videoRecorder.StartRecord());
    _recordTime = QTime::currentTime();
    _captureStatusLabel->setText("状态: 正在工作");
    _settingsBtn->setEnabled(false);
    _captureListWidget->setEnabled(false);
    _updateListBtn->setEnabled(false);
    _captureMethodBox->setEnabled(false);
    return true;
}

void AvRecorder::_StopStream()
{
    _audioRecorder.StopRecord();
    _videoRecorder.StopRecord();
    _avMuxer.Close();
    _captureStatusLabel->setText("状态: 正常");
    _settingsBtn->setEnabled(true);
    _captureListWidget->setEnabled(true);
    _updateListBtn->setEnabled(true);
    _captureMethodBox->setEnabled(true);
}

void AvRecorder::_UpdateCaptureList()
{
    _captureListWidget->clear();
    auto&& monitorList = MonitorFinder::GetList(true);
    for (auto&& monitor : monitorList) {
        _captureListWidget->addItem("屏幕: " + QString::fromStdWString(monitor.title));
    }
    auto&& windowList = WindowFinder::GetList(true);
    for (auto&& window : windowList) {
        _captureListWidget->addItem("窗口: " + QString::fromStdWString(window.title));
    }
}

void AvRecorder::_InitUi()
{
    setFont(QFont("Microsoft Yahei"));
    resize(800, 600);
    _videoWidget = new VideoWidget;
    auto hLayout = new QHBoxLayout;
    hLayout->addLayout(_InitAudioUi(), 2);
    hLayout->addLayout(_InitListUi(), 2);
    hLayout->addLayout(_InitOtherUi(), 1);
    _InitStatusBarUi();
    auto widget = new QWidget;
    auto layout = new QVBoxLayout;
    layout->addWidget(_videoWidget, 4);
    layout->addLayout(hLayout, 1);
    widget->setLayout(layout);
    this->setCentralWidget(widget);
    _UpdateCaptureList();
}

QVBoxLayout* AvRecorder::_InitListUi()
{
    auto layout = new QVBoxLayout;
    _captureListWidget = new QListWidget;
    layout->addWidget(_captureListWidget);
    return layout;
}

QVBoxLayout* AvRecorder::_InitAudioUi()
{
    _microphoneWidget = new AudioWidget;
    _speakerWidget = new AudioWidget;
    _microphoneWidget->SetName("麦克风");
    _speakerWidget->SetName("扬声器");
    auto layout = new QVBoxLayout;
    layout->addWidget(_microphoneWidget);
    layout->addWidget(_speakerWidget);
    return layout;
}

QVBoxLayout* AvRecorder::_InitOtherUi()
{
    _isDrawCursorBox = new QCheckBox("绘制鼠标指针");
    _isDrawCursorBox->setChecked(true);
    _isDrawCursorBox->setEnabled(false);
    _updateListBtn = new QPushButton("刷新窗口列表");
    _recordBtn = new QPushButton("开始录制");
    _recordBtn->setEnabled(false);
    _liveBtn = new QPushButton("开始直播");
    _liveBtn->setEnabled(false);
    _settingsBtn = new QPushButton("设置");
    auto layout = new QVBoxLayout;
    layout->addWidget(_isDrawCursorBox);
    layout->addWidget(_updateListBtn);
    layout->addWidget(_recordBtn);
    layout->addWidget(_liveBtn);
    layout->addWidget(_settingsBtn);
    return layout;
}

void AvRecorder::_InitStatusBarUi()
{
    auto layout = new QHBoxLayout;
    _videoEncodeLabel = new QLabel;
    auto hLayout = new QHBoxLayout;
    hLayout->addWidget(new QLabel("捕获方式:"));
    _captureMethodBox = new QComboBox;
    hLayout->addWidget(_captureMethodBox);
    _captureStatusLabel = new QLabel("状态: 正常");
    _captureTimeLabel = new QLabel("00:00:00");
    _videolossRate = new QLabel("丢帧率: 00.00%");
    _fpsLabel = new QLabel("FPS: 30");
    auto statusBar = this->statusBar();
    statusBar->layout()->setSpacing(20);
    statusBar->layout()->addWidget(_videoEncodeLabel);
    auto widget = new QWidget;
    widget->setLayout(hLayout);
    statusBar->layout()->addWidget(widget);
    statusBar->layout()->addWidget(_videolossRate);
    statusBar->layout()->addWidget(_captureStatusLabel);
    statusBar->layout()->addWidget(_captureTimeLabel);
    statusBar->layout()->addWidget(_fpsLabel);
}