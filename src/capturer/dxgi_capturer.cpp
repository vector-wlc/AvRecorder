#include "dxgi_capturer.h"
#include <windows.h>

DxgiCapturer::DxgiCapturer()
{
    ZeroMemory(&_dxgiOutDesc, sizeof(_dxgiOutDesc));
    ZeroMemory(&_desc, sizeof(_desc));
}

DxgiCapturer::~DxgiCapturer()
{
    Close();
}

bool DxgiCapturer::Open(int idx, int width, int height)
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
    hr = hDxgiAdapter->EnumOutputs(idx, &hDxgiOutput);
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
    __CheckBool(SUCCEEDED(_rgbToNv12.Init(_hDevice, _hContext)));
    _nv12Frame = Frame<MediaType::VIDEO>::Alloc(AV_PIX_FMT_NV12, width, height);
    _xrgbFrame = Frame<MediaType::VIDEO>::Alloc(AV_PIX_FMT_BGR0, width, height);
    __CheckBool(_nv12Frame);
    __CheckBool(_xrgbFrame);
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
    _nv12Buffers.Clear();
    _xrgbBuffers.Clear();
    _rgbToNv12.Cleanup();
    Free(_nv12Frame, [this] { av_frame_free(&_nv12Frame); });
    Free(_xrgbFrame, [this] { av_frame_free(&_xrgbFrame); });
    Free(_hDeskDupl, [this] { _hDeskDupl->Release(); });
    Free(_hDevice, [this] { _hDevice->Release(); });
    Free(_hContext, [this] { _hContext->Release(); });
}

HDC DxgiCapturer::GetHdc()
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

    srcImage->GetDesc(&_desc);

    // create a new staging buffer for fill frame image
    auto desc = _desc;
    desc.ArraySize = 1;
    desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.MipLevels = 1;
    desc.CPUAccessFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    hr = _hDevice->CreateTexture2D(&desc, nullptr, &_gdiImage);
    if (FAILED(hr)) {
        __DebugPrint("Create _gdiImage failed");
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

AVFrame* DxgiCapturer::GetFrame()
{
    if (!_isCaptureSuccess) {
        return nullptr;
    }
    _isCaptureSuccess = false;
    _hStagingSurf->ReleaseDC(nullptr);

    // 创建一个临时的纹理
    ID3D11Texture2D* tmpImage = nullptr;
    _desc.MiscFlags = 2050;
    __CheckNullptr(SUCCEEDED(_hDevice->CreateTexture2D(&_desc, nullptr, &tmpImage)));
    _hContext->CopyResource(tmpImage, _gdiImage);

    // 首先尝试创建 NV12 纹理
    AVFrame* frame = nullptr;
    auto tmpFormat = _desc.Format;
    _desc.Format = DXGI_FORMAT_NV12;
    if (GenNv12Frame(_hDevice, _hContext, _desc, tmpImage,
            _nv12Buffers, _nv12Frame, _rgbToNv12)) {
        frame = _nv12Frame;
    } else {
        _desc.Format = tmpFormat;
        GenRgbFrame(_hDevice, _hContext, _desc, _gdiImage,
            _xrgbBuffers, _xrgbFrame);
        frame = _xrgbFrame;
    }
    Free(_hStagingSurf, [this] { _hStagingSurf->Release(); });
    Free(tmpImage, [&tmpImage] { tmpImage->Release(); });
    Free(_gdiImage, [this] { _gdiImage->Release(); });

    return frame;
}