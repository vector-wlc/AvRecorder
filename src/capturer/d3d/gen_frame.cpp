/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-03-01 12:35:29
 * @Description:
 */

#include "gen_frame.h"
#include <winrt/base.h>

#undef min
#undef max

bool GenNv12Frame(ID3D11Device* device, ID3D11DeviceContext* ctx, const D3D11_TEXTURE2D_DESC& desc,
    ID3D11Texture2D* img, BufferFiller& buffers, AVFrame*& outFrame, RGBToNV12& rgbToNv12)
{
    winrt::com_ptr<ID3D11Texture2D> nv12Img = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, nv12Img.put()))) {
        return false;
    }
    __CheckBool(SUCCEEDED(rgbToNv12.Convert(img, nv12Img.get())));
    // 填充缓冲区
    __CheckBool(buffers.Fill(device, desc));

    ctx->CopyResource(buffers.GetCopy(), nv12Img.get());
    D3D11_MAPPED_SUBRESOURCE resource;
    __CheckBool(SUCCEEDED(ctx->Map(buffers.GetMap(), 0, D3D11_MAP_READ, 0, &resource)));
    auto height = std::min(outFrame->height, (int)resource.DepthPitch);
    auto width = outFrame->width;
    auto srcLinesize = resource.RowPitch;
    auto dstLinesize = outFrame->linesize[0];
    auto srcData = (uint8_t*)resource.pData;
    auto titleHeight = std::max(int(desc.Height - height), 0);
    auto copyLine = std::min(std::min(width, (int)srcLinesize), dstLinesize);
    auto border = (desc.Width - width) / 2;
    __mtx.lock();

    // Y
    int Ystart = (titleHeight - border) * srcLinesize + border;
    auto dstData = outFrame->data[0];
    for (int row = 0; row < height; ++row) {
        memcpy(dstData + row * dstLinesize, srcData + Ystart + row * srcLinesize, width);
    }

    // UV
    dstData = outFrame->data[1];
    int UVStart = srcLinesize * desc.Height + (titleHeight - border) / 2 * srcLinesize + border / 2 * 2;
    for (int row = 0; row < height / 2; ++row) {
        memcpy(dstData + row * dstLinesize, srcData + UVStart + row * srcLinesize, width);
    }

    __mtx.unlock();
    ctx->Unmap(buffers.GetMap(), 0);
    __CheckBool(buffers.Reset());
    return true;
}
bool GenRgbFrame(ID3D11Device* device, ID3D11DeviceContext* ctx, const D3D11_TEXTURE2D_DESC& desc,
    ID3D11Texture2D* img, BufferFiller& buffers, AVFrame*& outFrame)
{
    __CheckBool(buffers.Fill(device, desc));
    ctx->CopyResource(buffers.GetCopy(), img);
    D3D11_MAPPED_SUBRESOURCE resource;
    __CheckBool(SUCCEEDED(ctx->Map(buffers.GetMap(), 0, D3D11_MAP_READ, 0, &resource)));
    auto height = std::min(outFrame->height, (int)resource.DepthPitch);
    auto width = outFrame->width;
    auto srcLinesize = resource.RowPitch;
    auto dstLinesize = outFrame->linesize[0];
    auto srcData = (uint8_t*)resource.pData;
    auto dstData = outFrame->data[0];
    auto titleHeight = std::max(int(desc.Height - height), 0);
    auto copyLine = std::min(std::min(width * 4, (int)srcLinesize), dstLinesize);
    auto border = (desc.Width - width) / 2;
    __mtx.lock();
    for (int row = 0; row < height; ++row) {
        auto offset = (titleHeight + row - border) * srcLinesize + border * 4;
        memcpy(dstData + row * dstLinesize, srcData + offset, copyLine);
    }
    __mtx.unlock();
    ctx->Unmap(buffers.GetMap(), 0);
    __CheckBool(buffers.Reset());
    return true;
}