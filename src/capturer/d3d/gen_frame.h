/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-03-01 12:35:36
 * @Description:
 */
#ifndef __GEN_FRAME_H__
#define __GEN_FRAME_H__

#include <d3d11.h>
#include "buffer_filler.h"
#include "basic/frame.h"
#include "convert.h"

bool GenNv12Frame(ID3D11Device* device, ID3D11DeviceContext* ctx, const D3D11_TEXTURE2D_DESC& desc,
    ID3D11Texture2D* img, BufferFiller& buffers, AVFrame*& outFrame, RGBToNV12& rgbToNv12);
bool GenRgbFrame(ID3D11Device* device, ID3D11DeviceContext* ctx, const D3D11_TEXTURE2D_DESC& desc,
    ID3D11Texture2D* img, BufferFiller& buffers, AVFrame*& outFrame);
#endif