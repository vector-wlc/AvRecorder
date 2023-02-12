/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-03 15:51:31
 * @Description:
 */
#ifndef __VIDEO_LABEL_H__
#define __VIDEO_LABEL_H__
#include <QWidget>

class QPaintEvent;

class VideoWidget : public QWidget {
    Q_OBJECT
public:
    VideoWidget(QWidget* parent = nullptr);
    HWND GetHwnd() const;
    void SetScaleFixSize(int width, int height);

protected:
    virtual void resizeEvent(QResizeEvent* event) override;
    void _ResizeWithScaled();
    int _ratioWidth = 0;
    int _ratioHeight = 0;
    QWidget* _viewWidget = nullptr;
};

#endif