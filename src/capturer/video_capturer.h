/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-01 18:10:33
 * @Description:
 */
#ifndef __AV_CAPTURER_H__
#define __AV_CAPTURER_H__

#include "dxgi_capturer.h"
#include "gdi_capturer.h"

class VideoCapturer {
public:
    ~VideoCapturer();
    bool Open(HWND hwnd);
    AVFrame* GetFrame(bool isDrawCursor = true);
    void Close();
    int GetWidth() const;
    int GetHeight() const;
    bool IsUseDxgi() const;

private:
    bool _InitFrame(int width, int height);
    bool _GetHwndSize(HWND hwnd);
    void _DrawCursor(HDC hdc);
    DxgiCapturer _dxgiCapturer;
    GdiCapturer _gdiCapturer;
    AVFrame* _frameRgb = nullptr;
    AVFrame* _frameXrgb = nullptr;
    int _width = 0;
    int _height = 0;
    int _borderHeight = 0;
    int _borderWidth = 0;
    HWND _srcHwnd = nullptr;
    bool _isUseDxgi = false;
};
#endif
