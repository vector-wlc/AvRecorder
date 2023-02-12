#include "av_capturer.h"

bool AvCapturer::Open(HWND hwnd)
{
    Close();
    if (hwnd == nullptr) {
        __DebugPrint("hwnd can't be nullptr\n");
        return false;
    }
    _srcHwnd = hwnd;
    if (!_GetHwndSize(_srcHwnd)) {
        __DebugPrint("_GetHwndSize failed\n");
        return false;
    }
    _isUseDxgi = false;
    if (GetDesktopWindow() == hwnd) { // DXGI 是用于加速桌面录制的技术
        _isUseDxgi = _dxgiCapturer.Open();
        if (!_isUseDxgi) {
            _dxgiCapturer.Close();
        }
    }
    if (!_isUseDxgi) { // DXGI 无法使用，使用 GDI 截屏
        __DebugPrint("Use GDI\n");
        _gdiCapturer.Open(hwnd, _width, _height);
    } else {
        __DebugPrint("Use DXGI\n");
    }
    return _InitFrame(_width, _height);
}

AVFrame* AvCapturer::GetFrame(bool isDrawCursor)
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

bool AvCapturer::GetIsUseDxgi() const
{
    return _isUseDxgi;
}

void AvCapturer::Close()
{
    _isUseDxgi ? _dxgiCapturer.Close() : _gdiCapturer.Close();
    Free(_frameRgb, [this] { av_frame_free(&_frameRgb); });
    Free(_frameXrgb, [this] { av_frame_free(&_frameXrgb); });
}

AvCapturer::~AvCapturer()
{
    Close();
}

int AvCapturer::GetWidth() const
{
    return _width;
}
int AvCapturer::GetHeight() const
{
    return _height;
}

bool AvCapturer::_InitFrame(int width, int height)
{
    auto&& frame = _isUseDxgi ? _frameXrgb : _frameRgb;
    auto format = _isUseDxgi ? AV_PIX_FMT_BGR0 : AV_PIX_FMT_BGR24;
    frame = AllocFrame(format, width, height);
    if (frame == nullptr) {
        __DebugPrint("_AllocFrame failed\n");
        return false;
    }
    return true;
}

bool AvCapturer::_GetHwndSize(HWND hwnd)
{
    RECT rect;
    if (!GetClientRect(hwnd, &rect)) {
        __DebugPrint("GetClientRect failed\n");
        return false;
    }
    _width = (rect.right - rect.left);
    _height = (rect.bottom - rect.top);
    if (!GetWindowRect(hwnd, &rect)) {
        __DebugPrint("GetWindowRect failed\n");
        return false;
    }
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

void AvCapturer::_DrawCursor(HDC hdc)
{
    CURSORINFO ci;
    ci.cbSize = sizeof(CURSORINFO);
    if (!GetCursorInfo(&ci)) {
        __DebugPrint("GetCursorInfo failed\n");
        return;
    }
    RECT rect;
    GetWindowRect(_srcHwnd, &rect);
    rect.left += _borderWidth / 2;
    rect.right -= _borderWidth / 2;
    rect.top += _borderHeight - _borderWidth / 2;

    if (ci.flags == CURSOR_SHOWING) {
        // 将光标画到屏幕所在位置
        int x = ci.ptScreenPos.x - rect.left;
        int y = ci.ptScreenPos.y - rect.top;
        DrawIconEx(hdc, x, y, ci.hCursor, 0, 0, 0, NULL, DI_NORMAL | DI_COMPAT);
    }
}