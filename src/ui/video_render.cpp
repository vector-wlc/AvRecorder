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
    __CheckBool(SUCCEEDED(
        D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &sd, &_swapChain, &_device, nullptr, &_context)));
    _width = width;
    _height = height;
    __CheckBool(SUCCEEDED(_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0)));

    MWComPtr<ID3D11Texture2D> pBackBuffer;
    __CheckBool(SUCCEEDED(_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &pBackBuffer)));

    // Create a render-target view
    MWComPtr<ID3D11RenderTargetView> renderTargetView;
    __CheckBool(SUCCEEDED(_device->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &renderTargetView)));

    _context->OMSetRenderTargets(1, &renderTargetView, nullptr);

    return true;
}

void VideoRender::Close()
{
    Free(_swapChain, [this] { _swapChain->Release(); });
    Free(_device, [this] { _device->Release(); });
    Free(_context, [this] { _context->Release(); });
    _swConverter = nullptr;
    _hwConverter = nullptr;
}

bool VideoRender::_Convert(AVFrame* frame, ID3D11Texture2D* texture)
{
    if (_lastFmt != frame->format) {
        _swConverter = nullptr;
        _hwConverter = nullptr;
        _lastFmt = frame->format;
    }
    if (_HardwareConvert(frame, texture)) {
        return true;
    }
    __CheckBool(_SoftwareConvert(frame, texture));
    return true;
}

bool VideoRender::_SoftwareConvert(AVFrame* frame, ID3D11Texture2D* texture)
{
    if (_swConverter == nullptr) {
        auto fmt = AVPixelFormat(frame->format);
        _swConverter = std::make_unique<FfmpegConverter>(fmt, AV_PIX_FMT_RGBA);
        _swConverter->SetSize(_width, _height);
    }
    __CheckBool(_bufferFrame = _swConverter->Trans(frame));
    _context->UpdateSubresource(texture, 0, nullptr, _bufferFrame->data[0], _bufferFrame->linesize[0], 0);
    return true;
}

bool VideoRender::_HardwareConvert(AVFrame* frame, ID3D11Texture2D* texture)
{
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    switch (frame->format) {
    case AV_PIX_FMT_BGR0:
        desc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
        break;
    case AV_PIX_FMT_NV12:
        desc.Format = DXGI_FORMAT_NV12;
        break;
    default:
        return false;
    }
    MWComPtr<ID3D11Texture2D> tmpTexture = nullptr;
    __CheckBool(SUCCEEDED(_device->CreateTexture2D(&desc, nullptr, &tmpTexture)));

    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.ArraySize = 1;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.MipLevels = 1;
    MWComPtr<ID3D11Texture2D> cpuTexture = nullptr;
    __CheckBool(SUCCEEDED(_device->CreateTexture2D(&desc, nullptr, &cpuTexture)));
    D3D11_MAPPED_SUBRESOURCE resource;
    __CheckBool(SUCCEEDED(_context->Map(cpuTexture.Get(), 0, D3D11_MAP_WRITE, 0, &resource)));
    if (desc.Format == DXGI_FORMAT_NV12) {
        int srcRow = frame->linesize[0];
        int dstRow = resource.RowPitch;
        for (int row = 0; row < frame->height; ++row) { // Y
            memcpy((uint8_t*)resource.pData + row * dstRow,
                frame->data[0] + srcRow * row, frame->width);
        }
        for (int row = 0; row < frame->height / 2; ++row) { // UV
            memcpy((uint8_t*)resource.pData + (row + frame->height) * dstRow,
                frame->data[1] + srcRow * row, frame->width);
        }
    } else {
        int srcRow = frame->linesize[0];
        int dstRow = resource.RowPitch;
        for (int row = 0; row < frame->height; ++row) {
            memcpy((uint8_t*)resource.pData + row * dstRow,
                frame->data[0] + srcRow * row, frame->width * 4);
        }
    }
    _context->Unmap(cpuTexture.Get(), 0);
    _context->CopyResource(tmpTexture.Get(), cpuTexture.Get());

    if (_hwConverter == nullptr) {
        _hwConverter = std::make_unique<D3dConverter>();
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
        if (FAILED(_hwConverter->Open(_device, _context, outputColorSpace, inputColorSpace))) {
            return false;
        }
    }
    if (FAILED(_hwConverter->Convert(tmpTexture.Get(), texture))) {
        return false;
    }
    return true;
}

bool VideoRender::Render(AVFrame* frame)
{
    if (frame == nullptr) {
        __CheckBool(SUCCEEDED(_swapChain->Present(0, 0)));
        return true;
    }
    __CheckBool(_device != nullptr && _swapChain != nullptr && _context != nullptr);
    MWComPtr<ID3D11Texture2D> pBackBuffer;
    __CheckBool(SUCCEEDED(_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &pBackBuffer)));
    _Convert(frame, pBackBuffer.Get());
    __CheckBool(SUCCEEDED(_swapChain->Present(0, 0)));
    pBackBuffer->Release();
    return true;
}