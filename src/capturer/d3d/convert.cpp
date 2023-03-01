
#include "convert.h"
using namespace std;

#if !defined(SAFE_RELEASE)
#define SAFE_RELEASE(X) \
    if (X) {            \
        X->Release();   \
        X = nullptr;    \
    }
#endif

#if !defined(PRINTERR1)
#define PRINTERR1(x) printf(__FUNCTION__ ": Error 0x%08x at line %d in file %s\n", x, __LINE__, __FILE__);
#endif

#if !defined(PRINTERR)
#define PRINTERR(x, y) printf(__FUNCTION__ ": Error 0x%08x in %s at line %d in file %s\n", x, y, __LINE__, __FILE__);
#endif

/// Initialize Video Context
HRESULT RGBToNV12::Init(ID3D11Device* pDev, ID3D11DeviceContext* pCtx)
{
    m_pDev = pDev;
    m_pCtx = pCtx;
    m_pDev->AddRef();
    m_pCtx->AddRef();
    /// Obtain Video device and Video device context
    HRESULT hr = m_pDev->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&m_pVid);
    if (FAILED(hr)) {
        PRINTERR(hr, "QAI for ID3D11VideoDevice");
    }
    hr = m_pCtx->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&m_pVidCtx);
    if (FAILED(hr)) {
        PRINTERR(hr, "QAI for ID3D11VideoContext");
    }

    return hr;
}

/// Release all Resources
void RGBToNV12::Cleanup()
{
    for (auto& it : viewMap) {
        ID3D11VideoProcessorOutputView* pVPOV = it.second;
        pVPOV->Release();
    }
    SAFE_RELEASE(m_pVP);
    SAFE_RELEASE(m_pVPEnum);
    SAFE_RELEASE(m_pVidCtx);
    SAFE_RELEASE(m_pVid);
    SAFE_RELEASE(m_pCtx);
    SAFE_RELEASE(m_pDev);
}

/// Perform Colorspace conversion
HRESULT RGBToNV12::Convert(ID3D11Texture2D* pRGB, ID3D11Texture2D* pYUV)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC inDesc = {0};
    D3D11_TEXTURE2D_DESC outDesc = {0};
    pRGB->GetDesc(&inDesc);
    pYUV->GetDesc(&outDesc);

    /// Check if VideoProcessor needs to be reconfigured
    /// Reconfiguration is required if input/output dimensions have changed
    if (m_pVP) {
        if (m_inDesc.Width != inDesc.Width || m_inDesc.Height != inDesc.Height || m_outDesc.Width != outDesc.Width || m_outDesc.Height != outDesc.Height) {
            SAFE_RELEASE(m_pVPEnum);
            SAFE_RELEASE(m_pVP);
        }
    }

    if (!m_pVP) {
        /// Initialize Video Processor
        m_inDesc = inDesc;
        m_outDesc = outDesc;
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc = {
            D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
            {0, 0}, inDesc.Width, inDesc.Height,
            {0, 0}, outDesc.Width, outDesc.Height,
            D3D11_VIDEO_USAGE_PLAYBACK_NORMAL};
        hr = m_pVid->CreateVideoProcessorEnumerator(&contentDesc, &m_pVPEnum);
        if (FAILED(hr)) {
            PRINTERR(hr, "CreateVideoProcessorEnumerator");
        }
        hr = m_pVid->CreateVideoProcessor(m_pVPEnum, 0, &m_pVP);
        if (FAILED(hr)) {
            PRINTERR(hr, "CreateVideoProcessor");
        }
    }

    /// Obtain Video Processor Input view from input texture
    ID3D11VideoProcessorInputView* pVPIn = nullptr;
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputVD = {0, D3D11_VPIV_DIMENSION_TEXTURE2D, {0, 0}};
    hr = m_pVid->CreateVideoProcessorInputView(pRGB, m_pVPEnum, &inputVD, &pVPIn);
    if (FAILED(hr)) {
        PRINTERR(hr, "CreateVideoProcessInputView");
        return hr;
    }

    /// Obtain Video Processor Output view from output texture
    ID3D11VideoProcessorOutputView* pVPOV = nullptr;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ovD = {D3D11_VPOV_DIMENSION_TEXTURE2D};
    hr = m_pVid->CreateVideoProcessorOutputView(pYUV, m_pVPEnum, &ovD, &pVPOV);
    if (FAILED(hr)) {
        SAFE_RELEASE(pVPIn);
        PRINTERR(hr, "CreateVideoProcessorOutputView");
        return hr;
    }

    /// Create a Video Processor Stream to run the operation
    D3D11_VIDEO_PROCESSOR_STREAM stream = {TRUE, 0, 0, 0, 0, nullptr, pVPIn, nullptr};

    /// Perform the Colorspace conversion
    hr = m_pVidCtx->VideoProcessorBlt(m_pVP, pVPOV, 0, 1, &stream);
    if (FAILED(hr)) {
        SAFE_RELEASE(pVPIn);
        PRINTERR(hr, "VideoProcessorBlt");
        return hr;
    }
    SAFE_RELEASE(pVPIn);
    SAFE_RELEASE(pVPOV);
    return hr;
}
