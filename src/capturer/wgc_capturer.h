/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-24 18:21:15
 * @Description:
 */

#ifndef __WGC_CAPTURER_H__
#define __WGC_CAPTURER_H__
#include "wgc/pch.h"
#include "wgc/winrt.h"
#include "wgc/App.h"
#include "basic/frame.h"
#include <list>

class WgcCapturer {
public:
    void SetOutputFrame(AVFrame* frame)
    {
        _frame = frame;
    }
    void StartCapturerWindow(HWND hwnd);
    void StartCapturerMonitor(HMONITOR monitor);
    void SetDrawCursor(bool isDrawCursor) { _app->SetDrawCursor(isDrawCursor); }
    static void Init();
    static WgcCapturer* New();
    static void Delete(WgcCapturer* ptr);
    static void Uninit();
    void Close()
    {
        if (_app != nullptr) {
            _app->Close();
        }
    }

private:
    WgcCapturer();
    ~WgcCapturer();
    App* _app = nullptr;
    bool _isAppInit = false;
    AVFrame* _frame = nullptr;
    static std::list<WgcCapturer*> _capturers;
    static winrt::Windows::System::DispatcherQueue* queuePtr;
    static winrt::Windows::UI::Composition::ContainerVisual* rootPtr;
};
#endif