/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-05 14:31:02
 * @Description:
 */
#ifndef __GDI_CAPTURER_H__
#define __GDI_CAPTURER_H__

#include "dxgi_capturer.h"
#include <Windows.h>

class GdiCapturer {
public:
    bool Open(HWND hwnd, int width, int height);
    HDC CaptureImage(int borderWidth, int borderHeight);
    bool WriteImage(AVFrame* frame);
    void Close();
    ~GdiCapturer();

private:
    HDC _srcHdc = nullptr;
    HDC _dstHdc = nullptr;
    HBITMAP _bitmap = nullptr;
    BITMAPINFO _bitmapInfo;
    int _width = 0;
    int _height = 0;
};

#endif