/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-07 22:34:39
 * @Description:
 */
#include "video_render.h"
#include <wrl/client.h>

template <typename T>
using MWComPtr = Microsoft::WRL::ComPtr<T>;

#define __Str(exp) #exp
#define __D3dCall(retVal, ...)                \
    do {                                      \
        auto hr = __VA_ARGS__;                \
        if (FAILED(hr)) {                     \
            __DebugPrint(__Str(__VA_ARGS__)); \
            return retVal;                    \
        }                                     \
    } while (false)

VideoRender::VideoRender()
{
}
VideoRender::~VideoRender()
{
    Close();
}

bool VideoRender::Open(HWND hwnd, unsigned int width, unsigned int height)
{
    Close();
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 0;
    sd.BufferDesc.RefreshRate.Denominator = 0;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.Flags = 0;
    sd.BufferCount = 1;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    __D3dCall(false,
        D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &sd, &_swapChain, &_device, nullptr, &_context));

    __D3dCall(false, _swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));

    MWComPtr<ID3D11Texture2D> pBackBuffer;
    __D3dCall(false, _swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &pBackBuffer));

    // Create a render-target view
    MWComPtr<ID3D11RenderTargetView> renderTargetView;
    __D3dCall(false, _device->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &renderTargetView));

    _context->OMSetRenderTargets(1, &renderTargetView, nullptr);

    if (!_xrgbToArgb.SetSize(width, height)) {
        __DebugPrint("_XrgbToArgb.SetSize failed\n");
        return false;
    }
    if (!_rgbToArgb.SetSize(width, height)) {
        __DebugPrint("_rgbToArgb.SetSize failed\n");
        return false;
    }

    return true;
}

void VideoRender::Close()
{
    Free(_swapChain, [this] { _swapChain->Release(); });
    Free(_device, [this] { _device->Release(); });
    Free(_context, [this] { _context->Release(); });
}

bool VideoRender::Trans(AVFrame* frame)
{
    if (_device == nullptr || _swapChain == nullptr || _context == nullptr) {
        __DebugPrint("Call Open first\n");
        return false;
    }

    if (frame == nullptr) { // frame 为 nullptr 就是直接刷新上一帧
        return true;
    }

    _bufferFrame = frame->format == AV_PIX_FMT_BGR0 ? _xrgbToArgb.Trans(frame) : _rgbToArgb.Trans(frame);
    if (_bufferFrame == nullptr) {
        __DebugPrint("Trans failed\n");
        return false;
    }
    return true;
}

bool VideoRender::Render()
{
    if (_device == nullptr || _swapChain == nullptr || _context == nullptr) {
        __DebugPrint("Call Open first\n");
        return false;
    }
    if (_bufferFrame == nullptr) {
        __DebugPrint("_bufferFrame == nullptr\n");
        return false;
    }
    MWComPtr<ID3D11Texture2D> pBackBuffer;
    __D3dCall(false, _swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &pBackBuffer));
    _context->UpdateSubresource(pBackBuffer.Get(), 0, nullptr, _bufferFrame->data[0], _bufferFrame->linesize[0], 0);
    __D3dCall(false, _swapChain->Present(0, 0));
    return true;
}
