/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-21 09:51:51
 * @Description:
 */
#include "audio_render.h"
#include <QPainter>

AudioRender::AudioRender(QLabel* parent)
    : QLabel(parent)
{
}
void AudioRender::ShowVolume(float volume)
{
    float val = 0;
    if (volume > 0.001) {
        val = (20 * log10(volume) + 60) / 60;
    }
    auto diff = val - _lastShowVal;
    if (diff > 0.02) {
        diff = 0.02;
    }
    if (diff < -0.02) {
        diff = -0.02;
    }
    _lastShowVal += diff;
}

void AudioRender::paintEvent(QPaintEvent* event)
{
    int val = _lastShowVal * width();
    QPainter painter(this);
    QPen pen(Qt::green, height());
    painter.setPen(pen);
    painter.drawLine(0, 0, val, 0);
    pen.setColor(Qt::gray);
    painter.setPen(pen);
    painter.drawLine(val, 0, width(), 0);
}