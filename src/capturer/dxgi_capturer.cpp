#include "dxgi_capturer.h"
#include <windows.h>

DxgiCapturer::DxgiCapturer()
{
    ZeroMemory(&_dxgiOutDesc, sizeof(_dxgiOutDesc));
    ZeroMemory(&_textureDesc, sizeof(_textureDesc));
}

DxgiCapturer::~DxgiCapturer()
{
    Close();
}

bool DxgiCapturer::Open()
{
    Close();
    HRESULT hr = S_OK;
    _isAttached = false;

    if (_bInit) {
        return false;
    }

    // Driver types supported
    D3D_DRIVER_TYPE DriverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

    // Feature levels supported
    D3D_FEATURE_LEVEL FeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1};
    UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

    D3D_FEATURE_LEVEL FeatureLevel;

    // Create D3D device
    for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex) {
        hr = D3D11CreateDevice(nullptr, DriverTypes[DriverTypeIndex], nullptr, 0, FeatureLevels,
            NumFeatureLevels, D3D11_SDK_VERSION, &_hDevice, &FeatureLevel, &_hContext);
        if (SUCCEEDED(hr)) {
            break;
        }
    }
    __CheckBool(SUCCEEDED(hr));

    // Get DXGI device
    IDXGIDevice* hDxgiDevice = nullptr;
    __CheckBool(SUCCEEDED(_hDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&hDxgiDevice))));

    // Get DXGI adapter
    IDXGIAdapter* hDxgiAdapter = nullptr;
    hr = hDxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&hDxgiAdapter));
    Free(hDxgiDevice, [=] { hDxgiDevice->Release(); });
    __CheckBool(SUCCEEDED(hr));

    // Get output
    INT nOutput = 0;
    IDXGIOutput* hDxgiOutput = nullptr;
    hr = hDxgiAdapter->EnumOutputs(nOutput, &hDxgiOutput);
    Free(hDxgiAdapter, [=] { hDxgiAdapter->Release(); });
    __CheckBool(SUCCEEDED(hr));

    // get output description struct
    hDxgiOutput->GetDesc(&_dxgiOutDesc);

    // QI for Output 1
    IDXGIOutput1* hDxgiOutput1 = nullptr;
    hr = hDxgiOutput->QueryInterface(__uuidof(hDxgiOutput1), reinterpret_cast<void**>(&hDxgiOutput1));
    Free(hDxgiOutput, [=] { hDxgiOutput->Release(); });
    __CheckBool(SUCCEEDED(hr));

    // Create desktop duplication
    hr = hDxgiOutput1->DuplicateOutput(_hDevice, &_hDeskDupl);
    Free(hDxgiOutput1, [=] { hDxgiOutput1->Release(); });
    __CheckBool(SUCCEEDED(hr));

    // 初始化成功
    _bInit = true;
    return true;
}
void DxgiCapturer::Close()
{
    if (!_bInit) {
        return;
    }

    _bInit = false;
    Free(_hDeskDupl, [this] { _hDeskDupl->Release(); });
    Free(_hDevice, [this] { _hDevice->Release(); });
    Free(_hContext, [this] { _hContext->Release(); });
}

bool DxgiCapturer::ResetDevice()
{
    Close();
    return Open();
}

HDC DxgiCapturer::CaptureImage()
{
    _isCaptureSuccess = false;
    if (!_bInit) {
        return nullptr;
    }

    IDXGIResource* hDesktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;
    HRESULT hr = _hDeskDupl->AcquireNextFrame(0, &FrameInfo, &hDesktopResource);
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) { // 这里是因为当桌面没有动画更新时就会有一个错误值，不进行错误打印
            return nullptr;
        }
        return nullptr;
    }

    // query next frame staging buffer
    ID3D11Texture2D* srcImage = nullptr;
    hr = hDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&srcImage));
    Free(hDesktopResource, [=] { hDesktopResource->Release(); });
    __CheckNullptr(SUCCEEDED(hr));

    srcImage->GetDesc(&_textureDesc);

    // create a new staging buffer for fill frame image
    _textureDesc.ArraySize = 1;
    _textureDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
    _textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
    _textureDesc.SampleDesc.Count = 1;
    _textureDesc.SampleDesc.Quality = 0;
    _textureDesc.MipLevels = 1;
    _textureDesc.CPUAccessFlags = 0;
    _textureDesc.Usage = D3D11_USAGE_DEFAULT;
    hr = _hDevice->CreateTexture2D(&_textureDesc, nullptr, &_gdiImage);
    if (FAILED(hr)) {
        __DebugPrint("Create _gdiImage failed");
        Free(srcImage, [=] { srcImage->Release(); });
        Free(_hDeskDupl, [this] { _hDeskDupl->ReleaseFrame(); });
        return nullptr;
    }

    _textureDesc.ArraySize = 1;
    _textureDesc.BindFlags = 0;
    _textureDesc.MiscFlags = 0;
    _textureDesc.SampleDesc.Count = 1;
    _textureDesc.SampleDesc.Quality = 0;
    _textureDesc.MipLevels = 1;
    _textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    _textureDesc.Usage = D3D11_USAGE_STAGING;
    hr = _hDevice->CreateTexture2D(&_textureDesc, nullptr, &_dstImage);
    if (FAILED(hr)) {
        __DebugPrint("Create _dstImage failed");
        Free(srcImage, [=] { srcImage->Release(); });
        Free(_hDeskDupl, [this] { _hDeskDupl->ReleaseFrame(); });
        return nullptr;
    }

    // copy next staging buffer to new staging buffer
    _hContext->CopyResource(_gdiImage, srcImage);
    Free(srcImage, [=] { srcImage->Release(); });
    _hDeskDupl->ReleaseFrame();

    // create staging buffer for map bits
    _hStagingSurf = nullptr;
    hr = _gdiImage->QueryInterface(__uuidof(IDXGISurface), (void**)(&_hStagingSurf));
    if (FAILED(hr)) {
        __DebugPrint("_gdiImage->QueryInterface failed");
        Free(_gdiImage, [this] { _gdiImage->Release(); });
        return nullptr;
    }

    _isCaptureSuccess = true;
    HDC hdc = nullptr;
    // if GetDc is failed, the hdc is nullptr
    _hStagingSurf->GetDC(FALSE, &hdc);
    return hdc;
}

bool DxgiCapturer::WriteImage(AVFrame* frame)
{
    __CheckBool(frame);
    __CheckBool(_isCaptureSuccess);
    _isCaptureSuccess = false;
    _hStagingSurf->ReleaseDC(nullptr);
    _hContext->CopyResource(_dstImage, _gdiImage);

    // Copy from CPU access texture to bitmap buffer
    D3D11_MAPPED_SUBRESOURCE resource;
    UINT subresource = D3D11CalcSubresource(0, 0, 0);
    __CheckBool(SUCCEEDED(_hContext->Map(_dstImage, subresource, D3D11_MAP_READ_WRITE, 0, &resource)));

    int height = frame->height;
    int width = frame->width;
    int srcLinesize = resource.RowPitch;
    int dstLinesize = frame->linesize[0];
    auto srcData = (uint8_t*)resource.pData;
    auto dstData = frame->data[0];

    for (int row = 0; row < height; ++row) {
        memcpy(dstData + row * dstLinesize, srcData + row * srcLinesize, width * 4);
    }

    Free(_hStagingSurf, [this] { _hStagingSurf->Release(); });
    Free(_dstImage, [this] { _dstImage->Release(); });
    Free(_gdiImage, [this] { _gdiImage->Release(); });
    return true;
}