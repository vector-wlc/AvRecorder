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
#include "wgc_capturer.h"

class VideoCapturer {
public:
    enum Method {
        GDI,
        DXGI,
        WGC
    };

    enum Type {
        WINDOW,
        MONITOR
    };
    ~VideoCapturer();
    bool Open(HWND hwnd, Method method);
    bool Open(int monitorIdx, Method method);
    AVFrame* GetFrame();
    void SetDrawCursor(bool isDrawCursor);
    void Close();
    int GetWidth() const;
    int GetHeight() const;
    Method GetMethod() const { return _usingMethod; }

private:
    bool _InitFrame(int width, int height);
    bool _GetHwndSize(HWND hwnd);
    void _DrawCursor(HDC hdc);
    Method _usingMethod = WGC;
    RECT _rect;
    Type _type = MONITOR;
    DxgiCapturer* _dxgiCapturer = nullptr;
    GdiCapturer* _gdiCapturer = nullptr;
    WgcCapturer* _wgcCapturer = nullptr;
    AVFrame* _frameRgb = nullptr;
    AVFrame* _frameXrgb = nullptr;
    int _width = 0;
    int _height = 0;
    int _borderHeight = 0;
    int _borderWidth = 0;
    HWND _srcHwnd = nullptr;
    bool _isDrawCursor = true;
};
#endif
