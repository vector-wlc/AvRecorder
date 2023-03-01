/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-04 15:50:31
 * @Description:
 */
#ifndef __DXGI_CAPTURER_H__
#define __DXGI_CAPTURER_H__

#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdio.h>
#include "d3d/gen_frame.h"
class DxgiCapturer {
public:
    DxgiCapturer();
    ~DxgiCapturer();

public:
    bool Open(int idx, int width, int height);
    void Close();

public:
    HDC GetHdc();
    AVFrame* GetFrame();

private:
    bool _bInit = false;
    bool _isCaptureSuccess = false;

    ID3D11Device* _hDevice = nullptr;
    ID3D11DeviceContext* _hContext = nullptr;
    IDXGIOutputDuplication* _hDeskDupl = nullptr;
    IDXGISurface1* _hStagingSurf = nullptr;
    ID3D11Texture2D* _gdiImage = nullptr;
    DXGI_OUTPUT_DESC _dxgiOutDesc;
    D3D11_TEXTURE2D_DESC _desc;
    bool _isAttached = false;
    AVFrame* _xrgbFrame = nullptr;
    AVFrame* _nv12Frame = nullptr;
    BufferFiller _xrgbBuffers;
    BufferFiller _nv12Buffers;
    RGBToNV12 _rgbToNv12;
};

#endif
