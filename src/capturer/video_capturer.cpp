#include "video_capturer.h"
#include "basic/frame.h"
#include "capturer/finder.h"

bool VideoCapturer::Open(HWND hwnd, Method method)
{
    Close();
    __CheckBool(hwnd);
    _srcHwnd = hwnd;
    __CheckBool(_GetHwndSize(_srcHwnd));
    _usingMethod = method;
    _type = WINDOW;
    switch (method) {
    case WGC: {
        _wgcCapturer = WgcCapturer::New();
        _wgcCapturer->StartCapturerWindow(hwnd, _width, _height);
        break;
    }

    default: { // GDI
        _gdiCapturer = new GdiCapturer;
        __CheckBool(_gdiCapturer->Open(hwnd, _width, _height));
        break;
    }
    }

    return true;
}

bool VideoCapturer::Open(int monitorIdx, Method method)
{
    Close();
    auto&& monitorInfo = MonitorFinder::GetList()[monitorIdx];
    _rect = monitorInfo.rect;
    _borderHeight = 0;
    _borderWidth = 0;
    _width = _rect.right - _rect.left;
    _height = _rect.bottom - _rect.top;
    _usingMethod = method;
    _type = MONITOR;
    switch (method) {
    case WGC: {
        auto monitor = monitorInfo.monitor;
        _wgcCapturer = WgcCapturer::New();
        _wgcCapturer->StartCapturerMonitor(monitor, _width, _height);
        break;
    }

    default: { // DXGI
        _dxgiCapturer = new DxgiCapturer;
        __CheckBool(_dxgiCapturer->Open(_rect.left, _rect.top, _width, _height));
        break;
    }
    }
    return true;
}

AVFrame* VideoCapturer::GetFrame()
{
    switch (_usingMethod) {
    case WGC: // 该捕获方式自动就将鼠标画好了，我们不需要再自己画鼠标
        return _wgcCapturer->GetFrame();
    case DXGI: {
        auto hdc = _dxgiCapturer->GetHdc();
        if (_isDrawCursor && hdc) {
            _DrawCursor(hdc);
        }
        return _dxgiCapturer->GetFrame();
    }
    default: // GDI
        auto hdc = _gdiCapturer->GetHdc(_borderWidth, _borderHeight);
        if (_isDrawCursor && hdc) {
            _DrawCursor(hdc);
        }
        return _gdiCapturer->GetFrame();
    }
}

void VideoCapturer::SetDrawCursor(bool isDrawCursor)
{
    _isDrawCursor = isDrawCursor;
    if (_usingMethod == WGC) {
        _wgcCapturer->SetDrawCursor(_isDrawCursor);
    }
}

void VideoCapturer::Close()
{
    Free(_dxgiCapturer, [this] { _dxgiCapturer->Close(); delete _dxgiCapturer; });
    Free(_gdiCapturer, [this] { _gdiCapturer->Close(); delete _gdiCapturer; });
    Free(_wgcCapturer, [this] { _wgcCapturer->Close(); });
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

bool VideoCapturer::_GetHwndSize(HWND hwnd)
{
    RECT rect;
    __CheckBool(GetClientRect(hwnd, &rect));
    _rect = rect;
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
    int cursorX = ci.ptScreenPos.x;
    int cursorY = ci.ptScreenPos.y;

    if (cursorX > _rect.right || cursorX < _rect.left
        || cursorY > _rect.bottom || cursorY < _rect.top) {
        return; // 超出显示范围
    }

    if (ci.flags == CURSOR_SHOWING) {
        // 将光标画到屏幕所在位置
        int x = cursorX - _rect.left;
        int y = cursorY - _rect.top;
        __CheckNo(DrawIconEx(hdc, x, y, ci.hCursor, 0, 0, 0, NULL, DI_NORMAL | DI_COMPAT));
    }
}