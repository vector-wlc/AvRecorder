/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-26 10:43:22
 * @Description:
 */
#include "buffer_filler.h"
#include "basic/basic.h"

bool BufferFiller::Fill(ID3D11Device* device, D3D11_TEXTURE2D_DESC desc, int maxCnt)
{
    desc.ArraySize = 1;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.MipLevels = 1;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    if (_buffers.size() == maxCnt) {
        ID3D11Texture2D* dstImg = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &dstImg))) {
            return false;
        }
        _buffers[_mapIdx] = dstImg;
        _mapIdx = (_mapIdx + 1) % _buffers.size();
        return true;
    }
    while (_buffers.size() < maxCnt) {
        ID3D11Texture2D* dstImg = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &dstImg))) {
            break;
        }
        _buffers.push_back(dstImg);
    }
    __CheckBool(!_buffers.empty());
    _copyIdx = 0;
    _mapIdx = (_copyIdx + 1) % _buffers.size();
    return true;
}

bool BufferFiller::Reset()
{
    _buffers[_mapIdx]->Release();
    _buffers[_mapIdx] = nullptr;
    _copyIdx = (_copyIdx + 1) % _buffers.size();
    return true;
}

void BufferFiller::Clear()
{
    for (auto&& dstImg : _buffers) {
        Free(dstImg, [&dstImg] { dstImg->Release(); });
    }
    _buffers.clear();
}
