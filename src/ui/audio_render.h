/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-21 09:49:18
 * @Description:
 */
#ifndef __AUDIO_RENDER_H__
#define __AUDIO_RENDER_H__

// 这里直接使用 Qt 中的 QLabel 进行音量的渲染

#include <QLabel>

class AudioRender : public QLabel {
public:
    AudioRender(QLabel* parent = nullptr);
    void ShowVolume(float volume);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    float _lastShowVal = 0;
};
#endif