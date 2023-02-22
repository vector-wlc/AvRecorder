#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include "encoder/audio_encoder.h"
#include "encoder/video_encoder.h"
#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

class SettingsPage : public QDialog {
public:
    struct Param {
        Encoder<MediaType::AUDIO>::Param audioParam;
        Encoder<MediaType::VIDEO>::Param videoParam;
        std::string outputDir;
    };
    SettingsPage(Param* param, QWidget* parent = nullptr);

private:
    void _InitUi();
    void _InitConnect();
    void _WriteSettings();
    QGroupBox* _InitVideoUi();
    QGroupBox* _InitAudioUi();
    QGroupBox* _InitOutputUi();
    Param* _param = nullptr;
    QSpinBox* _videoBitRateBox = nullptr;
    QSpinBox* _videoFpsBox = nullptr;
    QComboBox* _videoEncoderBox = nullptr;
    QSpinBox* _audioBitRateBox = nullptr;
    QLineEdit* _fileDirEdit = nullptr;
    QPushButton* _selDirBtn = nullptr;
    QPushButton* _applyBtn = nullptr;
    QPushButton* _cancelBtn = nullptr;
    QPushButton* _yesBtn = nullptr;

    QHBoxLayout* _CreateDescription(std::string_view text, std::string_view textEx, QWidget* widget);
};

#endif