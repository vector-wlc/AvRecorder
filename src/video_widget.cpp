/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-03 18:01:32
 * @Description:
 */
#include "video_widget.h"
#include <QLayout>
#include <QResizeEvent>

VideoWidget::VideoWidget(QWidget* parent)
    : QWidget(parent)
{
    _viewWidget = new QWidget(this);
    _viewWidget->setUpdatesEnabled(false);
    _ratioWidth = width();
    _ratioHeight = height();
    _ResizeWithScaled();
}

HWND VideoWidget::GetHwnd() const
{
    return (HWND)_viewWidget->winId();
}

void VideoWidget::resizeEvent(QResizeEvent* event)
{
    _ResizeWithScaled();
}

void VideoWidget::SetScaleFixSize(int width, int height)
{
    _ratioWidth = width;
    _ratioHeight = height;
    _ResizeWithScaled();
}

void VideoWidget::_ResizeWithScaled()
{
    int viewWidth = this->width();
    int viewHeight = this->height();
    if (_ratioHeight <= 0 || _ratioWidth <= 0
        || viewWidth <= 0 || viewHeight <= 0) {
        _viewWidget->resize(1, 1);
        return;
    }
    if (viewWidth < float(_ratioWidth) / _ratioHeight * viewHeight) {
        // 宽度不足，则缩小高度
        viewHeight = _ratioHeight * viewWidth / _ratioWidth;
    } else {
        // 否则缩小宽度
        viewWidth = _ratioWidth * viewHeight / _ratioHeight;
    }

    // 窗口居中
    // 获取父窗口的中心位置
    // 然后将子窗口的中心位置设置为父窗口的中心位置
    int centralX = this->width() / 2;
    int centralY = this->height() / 2;
    int viewX = centralX - viewWidth / 2;
    int viewY = centralY - viewHeight / 2;
    _viewWidget->resize(viewWidth, viewHeight);
    _viewWidget->move(viewX, viewY);
}