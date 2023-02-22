/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-05 14:30:33
 * @Description:
 */
#include "gdi_capturer.h"

#include "basic/basic.h"

bool GdiCapturer::Open(HWND hwnd, int width, int height)
{
    Close();
    _width = width;
    _height = height;
    _srcHdc = GetWindowDC(hwnd);
    _dstHdc = CreateCompatibleDC(_srcHdc);
    _bitmap = CreateCompatibleBitmap(_srcHdc, width, height);
    SelectObject(_dstHdc, _bitmap);

    _bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    _bitmapInfo.bmiHeader.biPlanes = 1;
    _bitmapInfo.bmiHeader.biBitCount = 24;
    _bitmapInfo.bmiHeader.biWidth = width;
    _bitmapInfo.bmiHeader.biHeight = height;
    _bitmapInfo.bmiHeader.biCompression = BI_RGB;
    _bitmapInfo.bmiHeader.biSizeImage = width * height;

    // 创建缓存帧
    return true;
}

HDC GdiCapturer::CaptureImage(int borderWidth, int borderHeight)
{
    __CheckNullptr(
        BitBlt(_dstHdc, 0, 0, _width, _height,
            _srcHdc, borderWidth / 2, borderHeight - borderWidth / 2, SRCCOPY));

    return _dstHdc;
}

bool GdiCapturer::WriteImage(AVFrame* frame)
{
    auto linesize = frame->linesize[0];
    for (int row = 0; row < _height; ++row) {
        __CheckBool(GetDIBits(_dstHdc, _bitmap, _height - 1 - row, 1, frame->data[0] + row * linesize, &_bitmapInfo, DIB_RGB_COLORS));
    }
    return true;
}

void GdiCapturer::Close()
{
    Free(_dstHdc, [this] { DeleteObject(_dstHdc); });
    Free(_bitmap, [this] { DeleteObject(_bitmap); });
}

GdiCapturer::~GdiCapturer()
{
    Close();
}
