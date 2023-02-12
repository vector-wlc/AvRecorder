/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-02 21:33:51
 * @Description:
 */

#ifndef __VIDEO_RENDER_H__
#define __VIDEO_RENDER_H__
#include <d3d11.h>
#include <d3d9.h>
#include <stdint.h>
#include <stdio.h>
#include <tchar.h>

#include "pix_transformer.h"

class VideoRender {

public:
    VideoRender();
    ~VideoRender();

public:
    bool Open(HWND hwnd, unsigned int width, unsigned int height);
    void Close();
    bool Render();
    bool Trans(AVFrame* frame); // 将图片的格式转为 D3D 能渲染的格式

private:
    IDXGISwapChain* _swapChain = nullptr;
    ID3D11Device* _device = nullptr;
    ID3D11DeviceContext* _context = nullptr;
    PixTransformer<AV_PIX_FMT_BGR0, AV_PIX_FMT_RGBA> _xrgbToArgb;
    PixTransformer<AV_PIX_FMT_BGR24, AV_PIX_FMT_RGBA> _rgbToArgb;
    AVFrame* _bufferFrame = nullptr;
};

#endif