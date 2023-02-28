/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-20 19:18:17
 * @Description: 
 */

#include "audio_widget.h"
#include <QLayout>

AudioWidget::AudioWidget(QWidget* parent)
    : QWidget(parent)
{
    _CreateUi();
    _CreateConnect();
}

void AudioWidget::_CreateUi()
{
    auto hLayout = new QHBoxLayout;
    _nameLabel = new QLabel;
    _mutebox = new QCheckBox("静音");
    _render = new AudioRender;
    _volumeBox = new QDoubleSpinBox;
    _volumeBox->setMinimum(0);
    _volumeBox->setValue(1);
    hLayout->addWidget(_nameLabel);
    hLayout->addWidget(_mutebox);
    auto scaleLayout = new QHBoxLayout;
    scaleLayout->addWidget(new QLabel("调幅:"));
    scaleLayout->addWidget(_volumeBox);
    hLayout->addLayout(scaleLayout);
    auto vLayout = new QVBoxLayout;
    vLayout->addLayout(hLayout);
    vLayout->addWidget(_render);
    setLayout(vLayout);
}

void AudioWidget::_CreateConnect()
{
    connect(_mutebox, &QCheckBox::stateChanged, [this](int) {
        if (_mutebox->isChecked()) {
            emit SetVolumeScale(0);
            _volumeBox->setEnabled(false);
        } else {
            _volumeBox->setEnabled(true);
            emit SetVolumeScale(_volumeBox->value());
        }
    });

    void (QDoubleSpinBox::*valueChanged)(double) = &(QDoubleSpinBox::valueChanged);


    connect(_volumeBox, valueChanged, [this] {
        emit SetVolumeScale(_volumeBox->value());
    });
}