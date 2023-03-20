/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-26 10:10:47
 * @Description:
 */

#include "wgc_capturer.h"
#include <QWidget>

winrt::Windows::System::DispatcherQueue* WgcCapturer::queuePtr = nullptr;
winrt::Windows::UI::Composition::ContainerVisual* WgcCapturer::rootPtr = nullptr;
std::list<WgcCapturer*> WgcCapturer::_capturers;
QWidget* __widget = nullptr;

void WgcCapturer::Init()
{
    if (queuePtr != nullptr) {
        return;
    }
    // Init COM
    init_apartment(apartment_type::single_threaded);
    // Create a DispatcherQueue for our thread
    static auto controller = CreateDispatcherQueueController();
    // Initialize Composition
    static auto compositor = Compositor();
    __widget = new QWidget;
    // __widget->resize(800, 600);
    // __widget->show();
    static auto target = CreateDesktopWindowTarget(compositor, (HWND)__widget->winId());
    static auto root = compositor.CreateContainerVisual();
    root.RelativeSizeAdjustment({1.0f, 1.0f});
    target.Root(root);

    // Enqueue our capture work on the dispatcher
    static auto queue = controller.DispatcherQueue();
    queuePtr = &queue;
    rootPtr = &root;
    // 首先 New 一个 Capturer 备用
    New();
}

void WgcCapturer::Uninit()
{
    delete __widget;
    while (!_capturers.empty()) {
        delete *_capturers.begin();
        _capturers.erase(_capturers.begin());
    }
}

WgcCapturer* WgcCapturer::New()
{
    // 将上一个 new 好的对象返回，并重新预备一个新的
    if (_capturers.empty()) {
        _capturers.push_back(new WgcCapturer);
    }
    return *(--_capturers.end());
}

void WgcCapturer::Delete(WgcCapturer* ptr)
{
    // auto iter = std::find(_capturers.begin(), _capturers.end(), ptr);
    // if (iter == _capturers.end()) {
    //     return;
    // }
    // if (*iter != nullptr) {
    //     delete *iter;
    // }
    // _capturers.erase(iter);
}

WgcCapturer::WgcCapturer()
{
    _app = new App;
    _isAppInit = false;
    auto success = queuePtr->TryEnqueue([=]() -> void {
        _app->Initialize(*rootPtr);
        _isAppInit = true;
    });
    WINRT_VERIFY(success);
}

WgcCapturer::~WgcCapturer()
{
    if (_app) {
        delete _app;
        _app = nullptr;
    }
}

bool WgcCapturer::StartCapturerMonitor(HMONITOR monitor, int width, int height)
{
    return _app->StartCaptureMonitor(monitor, width, height);
}

bool WgcCapturer::StartCapturerWindow(HWND hwnd, int width, int height)
{
    return _app->StartCaptureWindow(hwnd, width, height);
}
