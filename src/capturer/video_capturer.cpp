#include "video_capturer.h"
#include "basic/frame.h"

bool VideoCapturer::Open(HWND hwnd)
{
    Close();
    __CheckBool(hwnd);
    _srcHwnd = hwnd;
    __CheckBool(_GetHwndSize(_srcHwnd));
    _isUseDxgi = false;
    if (GetDesktopWindow() == hwnd) { // DXGI 是用于加速桌面录制的技术
        _isUseDxgi = _dxgiCapturer.Open();
        if (!_isUseDxgi) {
            _dxgiCapturer.Close();
        }
    }
    if (!_isUseDxgi) { // DXGI 无法使用，使用 GDI 截屏
        __DebugPrint("Use GDI");
        __CheckBool(_gdiCapturer.Open(hwnd, _width, _height));
    } else {
        __DebugPrint("Use DXGI");
    }
    __CheckBool(_InitFrame(_width, _height));
    return true;
}

AVFrame* VideoCapturer::GetFrame(bool isDrawCursor)
{
    HDC hdc = _isUseDxgi ? _dxgiCapturer.CaptureImage() : _gdiCapturer.CaptureImage(_borderWidth, _borderHeight);
    if (hdc == nullptr) {
        return nullptr;
    }
    if (isDrawCursor) {
        _DrawCursor(hdc);
    }
    auto ret = _isUseDxgi ? _dxgiCapturer.WriteImage(_frameXrgb) : _gdiCapturer.WriteImage(_frameRgb);
    if (ret) {
        return _isUseDxgi ? _frameXrgb : _frameRgb;
    }
    return nullptr;
}

bool VideoCapturer::IsUseDxgi() const
{
    return _isUseDxgi;
}

void VideoCapturer::Close()
{
    _isUseDxgi ? _dxgiCapturer.Close() : _gdiCapturer.Close();
    Free(_frameRgb, [this] { av_frame_free(&_frameRgb); });
    Free(_frameXrgb, [this] { av_frame_free(&_frameXrgb); });
}

VideoCapturer::~VideoCapturer()
{
    Close();
}

int VideoCapturer::GetWidth() const
{
    return _width;
}
int VideoCapturer::GetHeight() const
{
    return _height;
}

bool VideoCapturer::_InitFrame(int width, int height)
{
    auto&& frame = _isUseDxgi ? _frameXrgb : _frameRgb;
    auto format = _isUseDxgi ? AV_PIX_FMT_BGR0 : AV_PIX_FMT_BGR24;
    __CheckBool(frame = Frame<MediaType::VIDEO>::Alloc(format, width, height));
    return true;
}

bool VideoCapturer::_GetHwndSize(HWND hwnd)
{
    RECT rect;
    __CheckBool(GetClientRect(hwnd, &rect));
    _width = (rect.right - rect.left);
    _height = (rect.bottom - rect.top);
    __CheckBool(GetWindowRect(hwnd, &rect));
    _borderHeight = rect.bottom - rect.top - _height;
    _borderWidth = rect.right - rect.left - _width;
    if (_borderHeight < 0) {
        _borderHeight = 0;
    }
    if (_borderWidth < 0) {
        _borderWidth = 0;
    }
    return true;
}

void VideoCapturer::_DrawCursor(HDC hdc)
{
    CURSORINFO ci;
    ci.cbSize = sizeof(CURSORINFO);
    __CheckNo(GetCursorInfo(&ci));
    RECT rect;
    __CheckNo(GetWindowRect(_srcHwnd, &rect));
    rect.left += _borderWidth / 2;
    rect.right -= _borderWidth / 2;
    rect.top += _borderHeight - _borderWidth / 2;

    if (ci.flags == CURSOR_SHOWING) {
        // 将光标画到屏幕所在位置
        int x = ci.ptScreenPos.x - rect.left;
        int y = ci.ptScreenPos.y - rect.top;
        __CheckNo(DrawIconEx(hdc, x, y, ci.hCursor, 0, 0, 0, NULL, DI_NORMAL | DI_COMPAT));
    }
}