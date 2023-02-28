/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-24 16:28:40
 * @Description:
 */
#pragma once

#include "basic/frame.h"
#include "capturer/buffer_filler.h"
#include <chrono>

class SimpleCapture {
public:
    SimpleCapture(
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device,
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item,
        AVFrame* outFrame);
    ~SimpleCapture() { Close(); }

    void StartCapture();
    winrt::Windows::UI::Composition::ICompositionSurface CreateSurface(
        winrt::Windows::UI::Composition::Compositor const& compositor);

    void SetDrawCursor(bool isDrawCursor) { m_session.IsCursorCaptureEnabled(isDrawCursor); }

    void Close();

private:
    void OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

    void CheckClosed()
    {
        if (m_closed.load() == true) {
            throw winrt::hresult_error(RO_E_CLOSED);
        }
    }

private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item {nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool {nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session {nullptr};
    winrt::Windows::Graphics::SizeInt32 m_lastSize;

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device {nullptr};
    winrt::com_ptr<IDXGISwapChain1> m_swapChain {nullptr};
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext {nullptr};

    std::atomic<bool> m_closed = false;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker m_frameArrived;
    AVFrame* m_outFrame = nullptr;
    BufferFiller m_bufferFiller;
};