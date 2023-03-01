/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-24 16:28:40
 * @Description:
 */
// D3D
#include <d2d1_3.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <wincodec.h>

#include "pch.h"
#include "App.h"
#include "basic/frame.h"

using namespace winrt;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::Graphics::Capture;

void App::Initialize(
    ContainerVisual const& root)
{
    auto queue = DispatcherQueue::GetForCurrentThread();

    m_compositor = root.Compositor();
    m_root = m_compositor.CreateContainerVisual();
    m_content = m_compositor.CreateSpriteVisual();
    m_brush = m_compositor.CreateSurfaceBrush();

    m_root.RelativeSizeAdjustment({1, 1});
    root.Children().InsertAtTop(m_root);

    m_content.AnchorPoint({0.5f, 0.5f});
    m_content.RelativeOffsetAdjustment({0.5f, 0.5f, 0});
    m_content.RelativeSizeAdjustment({1, 1});
    m_content.Size({-80, -80});
    m_content.Brush(m_brush);
    m_brush.HorizontalAlignmentRatio(0.5f);
    m_brush.VerticalAlignmentRatio(0.5f);
    m_brush.Stretch(CompositionStretch::Uniform);
    auto shadow = m_compositor.CreateDropShadow();
    shadow.Mask(m_brush);
    m_content.Shadow(shadow);
    m_root.Children().InsertAtTop(m_content);

    auto d3dDevice = CreateD3DDevice();
    auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
    m_device = CreateDirect3DDevice(dxgiDevice.get());
}

void App::Close()
{
    if (m_capture) {
        m_capture->Close();
        delete m_capture;
        m_capture = nullptr;
    }
}

void App::StartCaptureWindow(HWND hwnd, int width, int height)
{
    Close();
    auto item = CreateCaptureItemForWindow(hwnd);
    m_capture = new SimpleCapture(m_device, item, width, height);
    auto surface = m_capture->CreateSurface(m_compositor);
    m_brush.Surface(surface);
    m_capture->StartCapture();
}

void App::SetDrawCursor(bool isDrawCursor)
{
    if (m_capture == nullptr) {
        return;
    }
    m_capture->SetDrawCursor(isDrawCursor);
}

void App::StartCaptureMonitor(HMONITOR monitor, int width, int height)
{
    Close();
    auto item = CreateCaptureItemForMonitor(monitor);
    m_capture = new SimpleCapture(m_device, item, width, height);
    auto surface = m_capture->CreateSurface(m_compositor);
    m_brush.Surface(surface);
    m_capture->StartCapture();
}