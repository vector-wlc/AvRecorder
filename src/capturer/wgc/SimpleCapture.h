/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-24 16:28:40
 * @Description:
 */
#pragma once

#include <chrono>
#include "capturer/d3d/gen_frame.h"

class SimpleCapture {
public:
    SimpleCapture(
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device,
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item,
        int width, int height);
    ~SimpleCapture() { Close(); }

    void StartCapture();
    winrt::Windows::UI::Composition::ICompositionSurface CreateSurface(
        winrt::Windows::UI::Composition::Compositor const& compositor);

    void SetDrawCursor(bool isDrawCursor) { m_session.IsCursorCaptureEnabled(isDrawCursor); }

    void Close();

    AVFrame* GetFrame() const noexcept { return m_pixType == NV12 ? m_nv12Frame : m_xrgbFrame; }

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
    enum _PixType {
        NV12,
        RGB
    };

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item {nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool {nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session {nullptr};
    winrt::Windows::Graphics::SizeInt32 m_lastSize;

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device {nullptr};
    winrt::com_ptr<IDXGISwapChain1> m_swapChain {nullptr};
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext {nullptr};

    std::atomic<bool> m_closed = false;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker m_frameArrived;
    AVFrame* m_xrgbFrame = nullptr;
    AVFrame* m_nv12Frame = nullptr;
    BufferFiller m_xrgbBuffers;
    BufferFiller m_nv12Buffers;
    RGBToNV12 m_rgbToNv12;
    _PixType m_pixType;
    bool m_isCapture = true;
    int m_cnt = 5;
};