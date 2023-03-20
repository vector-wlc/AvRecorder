/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-24 16:28:40
 * @Description:
 */
#pragma once

#include "basic/frame.h"
#include "SimpleCapture.h"

class App {
public:
    App() { }
    ~App() { }

    void Initialize(
        winrt::Windows::UI::Composition::ContainerVisual const& root);

    bool StartCaptureWindow(HWND hwnd, int width, int height);
    bool StartCaptureMonitor(HMONITOR monitor, int width, int height);
    void SetDrawCursor(bool isDrawCursor);
    void Close();
    AVFrame* GetFrame() { return m_capture->GetFrame(); }

private:
    winrt::Windows::UI::Composition::Compositor m_compositor {nullptr};
    winrt::Windows::UI::Composition::ContainerVisual m_root {nullptr};
    winrt::Windows::UI::Composition::SpriteVisual m_content {nullptr};
    winrt::Windows::UI::Composition::CompositionSurfaceBrush m_brush {nullptr};

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device {nullptr};
    SimpleCapture* m_capture = nullptr;
};