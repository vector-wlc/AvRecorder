/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-21 14:29:50
 * @Description:
 */

#include "settings_page.h"
#include "encoder/video_encoder.h"
#include <QFileDialog>

SettingsPage::SettingsPage(Param* param, QWidget* parent)
    : QDialog(parent)
    , _param(param)
{
    setFont(QFont("Microsoft Yahei"));
    _InitUi();
    _InitConnect();
}

void SettingsPage::_InitConnect()
{
    connect(_applyBtn, &QPushButton::released, [this] {
        _WriteSettings();
    });

    connect(_cancelBtn, &QPushButton::released, [this] {
        this->close();
    });

    connect(_yesBtn, &QPushButton::released, [this] {
        _WriteSettings();
        this->close();
    });

    connect(_selDirBtn, &QPushButton::released, [this] {
        QString selectedDir = QFileDialog::getExistingDirectory(this, "选择输出目录", "./", QFileDialog::ShowDirsOnly);
        // 若目录路径不为空
        if (!selectedDir.isEmpty()) {
            // 显示选择的目录路径
            _fileDirEdit->setText(selectedDir);
        }
    });
}

void SettingsPage::_WriteSettings()
{
    _param->videoParam.bitRate = _videoBitRateBox->value() * 1000;
    _param->videoParam.fps = _videoFpsBox->value();
    _param->videoParam.name = _videoEncoderBox->currentText().toStdString();
    _param->audioParam.bitRate = _audioBitRateBox->value() * 1000;
    _param->outputDir = _fileDirEdit->text().toStdString();
    _param->liveUrl = _liveUrlEdit->text().toStdString();
    _param->liveName = _liveNameEdit->text().toStdString();
}

void SettingsPage::_InitUi()
{
    setWindowTitle("Settings");
    auto layout = new QVBoxLayout;
    layout->addWidget(_InitVideoUi());
    layout->addWidget(_InitAudioUi());
    layout->addWidget(_InitOutputUi());
    layout->addWidget(_InitLiveUi());
    auto hLayout = new QHBoxLayout;
    _applyBtn = new QPushButton("应用");
    _cancelBtn = new QPushButton("取消");
    _yesBtn = new QPushButton("确定");
    hLayout->setAlignment(Qt::AlignRight);
    hLayout->addWidget(_applyBtn);
    hLayout->addWidget(_cancelBtn);
    hLayout->addWidget(_yesBtn);
    layout->addLayout(hLayout);
    setLayout(layout);
}

QGroupBox* SettingsPage::_InitVideoUi()
{
    auto groupBox = new QGroupBox("视频");
    auto layout = new QVBoxLayout;
    _videoBitRateBox = new QSpinBox;
    _videoBitRateBox->setMinimum(0);
    _videoBitRateBox->setMaximum(INT_MAX);
    _videoBitRateBox->setValue(_param->videoParam.bitRate / 1000);
    _videoFpsBox = new QSpinBox;
    _videoFpsBox->setMinimum(0);
    _videoFpsBox->setMaximum(60);
    _videoFpsBox->setValue(_param->videoParam.fps);
    _videoEncoderBox = new QComboBox;
    auto&& encoders = Encoder<MediaType::VIDEO>::GetUsableEncoders();
    for (auto&& encoder : encoders) {
        _videoEncoderBox->addItem(encoder.c_str());
    }
    _videoEncoderBox->setCurrentText(_param->videoParam.name.c_str());
    layout->addLayout(_CreateDescription("比特率(kB):", "越高的比特率越清晰, 但越占用硬件资源", _videoBitRateBox));
    layout->addLayout(_CreateDescription("帧率:", "越高的帧率越流畅, 但越占用硬件资源", _videoFpsBox));
    layout->addLayout(_CreateDescription("编码器:", "libx264 为软件编码, CPU占用高但兼容性强, 其他为硬件编码, 效果与软件编码相反", _videoEncoderBox));
    groupBox->setLayout(layout);
    return groupBox;
}
QGroupBox* SettingsPage::_InitAudioUi()
{
    auto groupBox = new QGroupBox("音频");
    auto layout = new QVBoxLayout;
    _audioBitRateBox = new QSpinBox;
    _audioBitRateBox->setMinimum(0);
    _audioBitRateBox->setMaximum(INT_MAX);
    _audioBitRateBox->setValue(_param->audioParam.bitRate / 1000);
    layout->addLayout(_CreateDescription("比特率(kB):", "越高的比特率越清晰, 但越占用硬件资源", _audioBitRateBox));
    groupBox->setLayout(layout);
    return groupBox;
}

QGroupBox* SettingsPage::_InitOutputUi()
{
    auto groupBox = new QGroupBox("输出");
    auto layout = new QHBoxLayout;
    _fileDirEdit = new QLineEdit(_param->outputDir.c_str());
    _selDirBtn = new QPushButton("选择");
    layout->addWidget(_fileDirEdit);
    layout->addWidget(_selDirBtn);
    groupBox->setLayout(layout);
    return groupBox;
}

QGroupBox* SettingsPage::_InitLiveUi()
{
    auto groupBox = new QGroupBox("直播");
    auto layout = new QVBoxLayout;
    _liveUrlEdit = new QLineEdit(_param->liveUrl.c_str());
    _liveNameEdit = new QLineEdit(_param->liveName.c_str());
    auto liveUrlLayout = new QHBoxLayout();
    liveUrlLayout->addWidget(new QLabel("地址:"));
    liveUrlLayout->addWidget(_liveUrlEdit);
    auto liveNameLayout = new QHBoxLayout();
    liveNameLayout->addWidget(new QLabel("名称(密钥):"));
    liveNameLayout->addWidget(_liveNameEdit);
    layout->addLayout(liveUrlLayout);
    layout->addLayout(liveNameLayout);
    groupBox->setLayout(layout);
    return groupBox;
}
QHBoxLayout* SettingsPage::_CreateDescription(std::string_view text, std::string_view textEx, QWidget* widget)
{
    auto layout = new QHBoxLayout;
    auto label = new QLabel(text.data());
    label->setToolTip(textEx.data());
    layout->addWidget(label);
    layout->addWidget(widget);
    return layout;
}