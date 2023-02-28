
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

SimpleCapture::SimpleCapture(
    IDirect3DDevice const& device,
    GraphicsCaptureItem const& item,
    AVFrame* outFrame)
{
    m_item = item;
    m_device = device;
    m_outFrame = outFrame;

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
    m_bufferFiller.Clear();
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
        m_bufferFiller.Clear();
    }

#undef min
#undef max
    if (m_outFrame != nullptr) {
        auto frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        D3D11_TEXTURE2D_DESC desc;
        frameSurface->GetDesc(&desc);
        auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
        __CheckNo(m_bufferFiller.Fill(d3dDevice.get(), desc));
        m_d3dContext->CopyResource(m_bufferFiller.GetCopy(), frameSurface.get());
        D3D11_MAPPED_SUBRESOURCE resource;
        __CheckNo(SUCCEEDED(m_d3dContext->Map(m_bufferFiller.GetMap(), 0, D3D11_MAP_READ, 0, &resource)));
        auto height = std::min(m_outFrame->height, (int)resource.DepthPitch);
        auto width = m_outFrame->width;
        auto srcLinesize = resource.RowPitch;
        auto dstLinesize = m_outFrame->linesize[0];
        auto srcData = (uint8_t*)resource.pData;
        auto dstData = m_outFrame->data[0];
        auto titleHeight = std::max(int(desc.Height - height), 0);
        auto copyLine = std::min(std::min(width * 4, (int)srcLinesize), dstLinesize);
        auto border = (desc.Width - width) / 2;
        __mtx.lock();
        for (int row = 0; row < height; ++row) {
            auto offset = (titleHeight + row - border) * srcLinesize + border * 4;
            memcpy(dstData + row * dstLinesize, srcData + offset, copyLine);
        }
        __mtx.unlock();

        m_d3dContext->Unmap(m_bufferFiller.GetMap(), 0);
        // com_ptr<ID3D11Texture2D> backBuffer;
        // check_hresult(m_swapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), backBuffer.put_void()));
        // m_d3dContext->CopyResource(backBuffer.get(), m_bufferFiller.GetMap());
        __CheckNo(m_bufferFiller.Reset());
    }

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
