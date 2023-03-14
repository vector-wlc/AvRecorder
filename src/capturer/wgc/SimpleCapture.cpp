
// D3D
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <d2d1_3.h>
#include <wincodec.h>

#include "pch.h"
#include "SimpleCapture.h"
#include "basic/basic.h"

using namespace winrt;
using namespace Windows;
using namespace Windows::Foundation;
using namespace Windows::System;
using namespace Windows::Graphics;
using namespace Windows::Graphics::Capture;
using namespace Windows::Graphics::DirectX;
using namespace Windows::Graphics::DirectX::Direct3D11;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Composition;

#undef min
#undef max

SimpleCapture::SimpleCapture(
    IDirect3DDevice const& device,
    GraphicsCaptureItem const& item,
    int width, int height)
{
    m_item = item;
    m_device = device;

    // Set up
    auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
    d3dDevice->GetImmediateContext(m_d3dContext.put());
    auto size = m_item.Size();

    m_swapChain = CreateDXGISwapChain(
        d3dDevice,
        static_cast<uint32_t>(size.Width),
        static_cast<uint32_t>(size.Height),
        static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized),
        2);

    // Create framepool, define pixel format (DXGI_FORMAT_B8G8R8A8_UNORM), and frame size.
    m_framePool = Direct3D11CaptureFramePool::Create(
        m_device,
        DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2,
        size);

    m_session = m_framePool.CreateCaptureSession(m_item);
    m_lastSize = size;
    m_frameArrived = m_framePool.FrameArrived(auto_revoke, {this, &SimpleCapture::OnFrameArrived});

    // Set ColorSpace
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE inputColorSpace;
    inputColorSpace.Usage = 1;
    inputColorSpace.RGB_Range = 0;
    inputColorSpace.YCbCr_Matrix = 1;
    inputColorSpace.YCbCr_xvYCC = 0;
    inputColorSpace.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;

    D3D11_VIDEO_PROCESSOR_COLOR_SPACE outputColorSpace;
    outputColorSpace.Usage = 0;
    outputColorSpace.RGB_Range = 0;
    outputColorSpace.YCbCr_Matrix = 1;
    outputColorSpace.YCbCr_xvYCC = 0;
    outputColorSpace.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
    m_rgbToNv12.Open(d3dDevice.get(), m_d3dContext.get(), inputColorSpace, outputColorSpace);
    m_nv12Frame = Frame<MediaType::VIDEO>::Alloc(AV_PIX_FMT_NV12, width, height);
    m_xrgbFrame = Frame<MediaType::VIDEO>::Alloc(AV_PIX_FMT_BGR0, width, height);
    __CheckNo(m_nv12Frame);
    __CheckNo(m_xrgbFrame);
    m_isCapture = true;
    m_cnt = 5;
}

// Start sending capture frames
void SimpleCapture::StartCapture()
{
    CheckClosed();
    m_session.StartCapture();
}

ICompositionSurface SimpleCapture::CreateSurface(
    Compositor const& compositor)
{
    CheckClosed();
    return CreateCompositionSurfaceForSwapChain(compositor, m_swapChain.get());
}

// Process captured frames
void SimpleCapture::Close()
{
    auto expected = false;
    if (m_closed.compare_exchange_strong(expected, true)) {
        m_frameArrived.revoke();
        m_framePool.Close();
        m_session.Close();
        m_swapChain = nullptr;
        m_framePool = nullptr;
        m_session = nullptr;
        m_item = nullptr;
    }
    m_nv12Buffers.Clear();
    m_xrgbBuffers.Clear();
    m_rgbToNv12.Close();
    Free(m_nv12Frame, [this] { av_frame_free(&m_nv12Frame); });
    Free(m_xrgbFrame, [this] { av_frame_free(&m_xrgbFrame); });
}

void SimpleCapture::OnFrameArrived(
    Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const&)
{
    auto newSize = false;
    auto frame = sender.TryGetNextFrame();
    auto frameContentSize = frame.ContentSize();
    if (frameContentSize.Width != m_lastSize.Width || frameContentSize.Height != m_lastSize.Height) {
        // The thing we have been capturing has changed size.
        // We need to resize our swap chain first, then blit the pixels.
        // After we do that, retire the frame and then recreate our frame pool.
        newSize = true;
        m_lastSize = frameContentSize;
        m_swapChain->ResizeBuffers(
            2,
            static_cast<uint32_t>(m_lastSize.Width),
            static_cast<uint32_t>(m_lastSize.Height),
            static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized),
            0);
        m_nv12Buffers.Clear();
        m_xrgbBuffers.Clear();
    }
    if (m_cnt > 0) {
        --m_cnt;
    }
    m_isCapture = m_isCapture && !newSize || m_cnt > 0;
    if (m_isCapture) {
        auto frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        D3D11_TEXTURE2D_DESC desc;
        frameSurface->GetDesc(&desc);
        auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);

        // 首先尝试创建 NV12 纹理
        auto tmpFormat = desc.Format;
        desc.Format = DXGI_FORMAT_NV12;
        if (GenNv12Frame(d3dDevice.get(), m_d3dContext.get(), desc, frameSurface.get(),
                m_nv12Buffers, m_nv12Frame, m_rgbToNv12)) {
            m_pixType = _PixType::NV12;
        } else {
            desc.Format = tmpFormat;
            GenRgbFrame(d3dDevice.get(), m_d3dContext.get(), desc, frameSurface.get(),
                m_xrgbBuffers, m_xrgbFrame);
            m_pixType = _PixType::RGB;
        }
    }

    // com_ptr<ID3D11Texture2D> backBuffer;
    // check_hresult(m_swapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), backBuffer.put_void()));
    // m_d3dContext->CopyResource(backBuffer.get(), m_bufferFiller.GetMap());

    // DXGI_PRESENT_PARAMETERS presentParameters = {0};
    // auto hr = m_swapChain->Present1(1, 0, &presentParameters);

    if (newSize) {
        m_framePool.Recreate(
            m_device,
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            m_lastSize);
    }
}
