/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-20 19:18:35
 * @Description:
 */
#ifndef __AUDIO_WIDGET_H__
#define __AUDIO_WIDGET_H__

#include "audio_render.h"
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>

class AudioWidget : public QWidget {
    Q_OBJECT
public:
    AudioWidget(QWidget* parent = nullptr);
    void ShowVolume(float volume)
    {
        _render->ShowVolume(volume);
        _render->update();
    }
    void SetName(const std::string& name)
    {
        _nameLabel->setText(name.c_str());
    }

    double GetVolume()
    {
        return _mutebox->isChecked() ? 0 : _volumeBox->value();
    }

private:
    void _CreateUi();
    void _CreateConnect();
    QLabel* _nameLabel = nullptr;
    AudioRender* _render = nullptr;
    QCheckBox* _mutebox = nullptr;
    QDoubleSpinBox* _volumeBox = nullptr;
    float _lastShowVal = 0;

signals:
    void SetVolumeScale(float scale);
};

#endif