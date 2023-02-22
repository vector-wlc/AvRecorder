/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2020-02-03 10:00:08
 * @Description:
 */

#include "av_recorder.h"
#include <QDateTime>
#include <QStatusBar>

AvRecorder::AvRecorder(QWidget* parent)
    : QMainWindow(parent)
{
    _settingsParam.audioParam.bitRate = 160'000;
    _settingsParam.videoParam.bitRate = 8000'000;
    _settingsParam.videoParam.fps = 30;
    _settingsParam.videoParam.name = Encoder<MediaType::VIDEO>::GetUsableEncoders().front();
    _settingsParam.outputDir = ".";

    _InitUi();
    _lastHwnd = GetDesktopWindow();
    _StartCapture(_lastHwnd);
    _StartPreview();
    _InitConnect();
}

AvRecorder::~AvRecorder()
{
    _StopRecord();
    _StopPreview();
    _StopCapture();
}

void AvRecorder::_StartCapture(HWND hwnd)
{
    __CheckNo(_audioRecorder.Open({AudioCapturer::Microphone, AudioCapturer::Speaker}, _settingsParam.audioParam));
    __CheckNo(_videoRecorder.Open(hwnd, _settingsParam.videoParam));
    _microphoneWidget->setEnabled(_audioRecorder.GetCaptureInfo(MICROPHONE_INDEX)->isUsable);
    _speakerWidget->setEnabled(_audioRecorder.GetCaptureInfo(SPEAKER_INDEX)->isUsable);
    _captureMethodLabel->setText(_videoRecorder.IsUseDxgi() ? "捕获方式: DXGI" : "捕获方式: GDI");
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
        __CheckNo(_videoRender.Trans(frame));
        __CheckNo(_videoRender.Render());
    });

    // 其他的渲染就没啥要求了
    // 直接使用 QTimer
    _otherTimer.callOnTimeout([this] {
        if (windowState() == Qt::WindowMinimized) {
            return;
        }
        // 音频
        auto volume = _audioRecorder.GetCaptureInfo(MICROPHONE_INDEX)->meanVolume;
        _microphoneWidget->ShowVolume(volume);
        volume = _audioRecorder.GetCaptureInfo(SPEAKER_INDEX)->meanVolume;
        _speakerWidget->ShowVolume(volume);
        // 状态栏
        if (_isRecord) {
            int interval = _recordTime.secsTo(QTime::currentTime());
            int sec = interval % 60;
            interval /= 60;
            int minute = interval % 60;
            int hour = interval / 60;
            _captureTimeLabel->setText(
                QString("%1:%2:%3").arg(hour, 2, 10, QChar('0')).arg(minute, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0')));
            std::string text;
            if (_avMuxer.IsEncodeOverload() || _videoRecorder.IsEncodeOverload()) {
                text += "编码过载, ";
            }
            if (_videoRecorder.IsCaptureOverload()) {
                text += "捕获过载, ";
            }
            if (!text.empty()) {
                _captureStatusLabel->setText("状态: " + QString(text.c_str()) + "请调低录制参数设置");
            }
        } else if (_captureTimeLabel->text() != "00:00:00") {
            _captureTimeLabel->setText("00:00:00");
        }
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

void AvRecorder::_StartRecord()
{
    auto fileName = _settingsParam.outputDir;
    if (fileName.back() != '\\') {
        fileName.push_back('\\');
    }
    fileName += QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss").toStdString() + ".mp4";
    __CheckNo(_avMuxer.Open(fileName));
    __CheckNo(_audioRecorder.LoadMuxer(_avMuxer));
    __CheckNo(_videoRecorder.LoadMuxer(_avMuxer));
    __CheckNo(_avMuxer.WriteHeader());
    __CheckNo(_audioRecorder.StartRecord());
    __CheckNo(_videoRecorder.StartRecord());
    _recordTime = QTime::currentTime();
    _captureStatusLabel->setText("状态: 正在录制");
    _settingsBtn->setEnabled(false);
    _windowListWidget->setEnabled(false);
    _updateListBtn->setEnabled(false);
    _isRecord = true;
}

void AvRecorder::_StopRecord()
{
    _isRecord = false;
    _audioRecorder.StopRecord();
    _videoRecorder.StopRecord();
    _avMuxer.Close();
    _captureStatusLabel->setText("状态: 正常");
    _settingsBtn->setEnabled(true);
    _windowListWidget->setEnabled(true);
    _updateListBtn->setEnabled(true);
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

    connect(_microphoneWidget, &AudioWidget::SetVolumeScale, [this](float scale) {
        _audioRecorder.SetVolumeScale(scale, MICROPHONE_INDEX);
    });
    connect(_speakerWidget, &AudioWidget::SetVolumeScale, [this](float scale) {
        _audioRecorder.SetVolumeScale(scale, SPEAKER_INDEX);
    });
    connect(_updateListBtn, &QPushButton::released, [this] {
        _UpdateWindowList();
    });
    connect(_windowListWidget, &QListWidget::currentTextChanged, [this](const QString& text) {
        HWND hwnd = text == "桌面" ? GetDesktopWindow() : FindWindowW(nullptr, text.toStdWString().c_str());
        if (hwnd == nullptr || hwnd == _lastHwnd) {
            return;
        }
        _lastHwnd = hwnd;
        _StopPreview();
        _StopCapture();
        _StartCapture(_lastHwnd);
        _StartPreview();
    });
    connect(_settingsBtn, &QPushButton::released, [this] {
        auto settingsPage = std::make_unique<SettingsPage>(&_settingsParam, this);
        settingsPage->exec();
        _StopPreview();
        _StopCapture();
        _StartCapture(_lastHwnd);
        _StartPreview();
    });
    connect(_isDrawCursorBox, &QCheckBox::stateChanged, [this] {
        _videoRecorder.SetIsDrawCursor(_isDrawCursorBox->isChecked());
    });
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) // 回调函数
{
    wchar_t szTitle[200];
    wchar_t szClass[200];
    auto windowListWidget = (QListWidget*)lParam;
    GetWindowTextW(hwnd, szTitle, sizeof(szTitle) / sizeof(wchar_t));     // 获取窗口名称
    GetClassNameW(hwnd, szClass, sizeof(szClass) / sizeof(wchar_t));      // 窗口类
    if (szTitle[0] != '\0' && IsWindowVisible(hwnd) && !IsIconic(hwnd)) { // 判断窗口标题不为空，并且窗口可见
        windowListWidget->addItem(QString::fromWCharArray(szTitle));      // 添加到临时list中
    }
    return TRUE;
}

void AvRecorder::_UpdateWindowList()
{
    _windowListWidget->clear();
    _windowListWidget->addItem("桌面");
    _windowListWidget->setCurrentItem(0);
    LPARAM lParam = (LPARAM)_windowListWidget;
    EnumWindows(EnumWindowsProc, lParam);
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
    _UpdateWindowList();
}

QVBoxLayout* AvRecorder::_InitListUi()
{
    auto layout = new QVBoxLayout;
    _windowListWidget = new QListWidget;
    layout->addWidget(_windowListWidget);
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
    _updateListBtn = new QPushButton("刷新窗口列表");
    _recordBtn = new QPushButton("开始录制");
    _settingsBtn = new QPushButton("设置");
    auto layout = new QVBoxLayout;
    layout->addWidget(_isDrawCursorBox);
    layout->addWidget(_updateListBtn);
    layout->addWidget(_recordBtn);
    layout->addWidget(_settingsBtn);
    return layout;
}

void AvRecorder::_InitStatusBarUi()
{
    auto layout = new QHBoxLayout;
    _videoEncodeLabel = new QLabel;
    _captureMethodLabel = new QLabel("捕获方式: GDI");
    _captureStatusLabel = new QLabel("状态: 正常");
    _captureTimeLabel = new QLabel("00:00:00");
    _fpsLabel = new QLabel("FPS: 30");
    auto statusBar = this->statusBar();
    statusBar->layout()->setSpacing(20);
    statusBar->layout()->addWidget(_videoEncodeLabel);
    statusBar->layout()->addWidget(_captureMethodLabel);
    statusBar->layout()->addWidget(_captureStatusLabel);
    statusBar->layout()->addWidget(_captureTimeLabel);
    statusBar->layout()->addWidget(_fpsLabel);
}