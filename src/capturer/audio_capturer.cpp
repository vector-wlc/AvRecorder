
#include "audio_capturer.h"

#include "basic/basic.h"
#include <iostream>

#define DEFAULT_SAMPLE_RATE 48000        // 默认采样率:48kHz
#define DEFAULT_BITS_PER_SAMPLE 16       // 默认位深:16bit
#define DEFAULT_CHANNELS 1               // 默认音频通道数:1
#define DEFAULT_AUDIO_PACKET_INTERVAL 10 // 默认音频包发送间隔:10ms

bool AudioCapturer::Init(Type deviceType, CallBack callback, void* userInfo)
{
    Stop();
    _userInfo = userInfo;
    _callback = callback;
    _deviceType = deviceType;
    __CheckBool(_CreateDeviceEnumerator(&_pDeviceEnumerator));
    __CheckBool(_CreateDevice(_pDeviceEnumerator, &_pDevice));
    __CheckBool(_CreateAudioClient(_pDevice, &_pAudioClient));

    if (_IsFormatSupported(_pAudioClient)) {
        __CheckBool(_GetPreferFormat(_pAudioClient, &_formatex));
    }
    __CheckBool(_InitAudioClient(_pAudioClient, &_formatex));
    __CheckBool(_CreateAudioCaptureClient(_pAudioClient, &_pAudioCaptureClient));
    _isInit = true;
    return true;
}

bool AudioCapturer::Start()
{
    __CheckBool(_isInit);
    _loopFlag = true;
    // 用于强制打开扬声器
    PlaySoundA("./rc/mute.wav", nullptr, SND_FILENAME | SND_ASYNC | SND_LOOP);
    _captureThread = new std::thread(
        [this] { _ThreadRun(_pAudioClient, _pAudioCaptureClient); });
    return true;
}

void AudioCapturer::Stop()
{
    // CoUninitialize();
    _isInit = false;
    _loopFlag = false;
    Free(_captureThread, [this] {
        _captureThread->join();
        delete _captureThread;
    });
    Free(_pAudioCaptureClient, [this] { _pAudioCaptureClient->Release(); });
    if (_pAudioClient != nullptr) {
        _pAudioClient->Stop();
    }
    PlaySoundA(nullptr, nullptr, SND_FILENAME | SND_ASYNC | SND_LOOP);

    Free(_pAudioClient, [this] { _pAudioClient->Release(); });
    Free(_pDevice, [this] { _pDevice->Release(); });
    Free(_pDeviceEnumerator, [this] { _pDeviceEnumerator->Release(); });
}

bool AudioCapturer::_CreateDeviceEnumerator(IMMDeviceEnumerator** enumerator)
{
    // __CheckBool(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)));
    // __CheckBool(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
    __CheckBool(SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(enumerator))));
    return true;
}
bool AudioCapturer::_CreateDevice(IMMDeviceEnumerator* enumerator, IMMDevice** device)
{
    EDataFlow enDataFlow = _deviceType == Microphone ? eCapture : eRender;
    ERole enRole = eConsole;
    __CheckBool(SUCCEEDED(enumerator->GetDefaultAudioEndpoint(enDataFlow, enRole, device)));
    return true;
}
bool AudioCapturer::_CreateAudioClient(IMMDevice* device, IAudioClient** audioClient)
{
    __CheckBool(SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
        (void**)audioClient)));
    return true;
}
bool AudioCapturer::_IsFormatSupported(IAudioClient* audioClient)
{
    memset(&_formatex, 0, sizeof(_formatex));
    WAVEFORMATEX* format = &_formatex.Format;
    format->nSamplesPerSec = DEFAULT_SAMPLE_RATE;
    format->wBitsPerSample = DEFAULT_BITS_PER_SAMPLE;
    format->nChannels = DEFAULT_CHANNELS;

    WAVEFORMATEX* closestMatch = nullptr;

    HRESULT hr = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
        format, &closestMatch);
    if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) // 0x88890008
    {
        if (closestMatch == nullptr) // 如果找不到最相近的格式，closestMatch可能为nullptr
        {
            return false;
        }

        format->nSamplesPerSec = closestMatch->nSamplesPerSec;
        format->wBitsPerSample = closestMatch->wBitsPerSample;
        format->nChannels = closestMatch->nChannels;

        return true;
    }

    return false;
}
bool AudioCapturer::_GetPreferFormat(IAudioClient* audioClient,
    WAVEFORMATEXTENSIBLE* formatex)
{
    WAVEFORMATEX* format = nullptr;
    __CheckBool(SUCCEEDED(audioClient->GetMixFormat(&format)));
    formatex->Format.nSamplesPerSec = format->nSamplesPerSec;
    formatex->Format.wBitsPerSample = format->wBitsPerSample;
    formatex->Format.nChannels = format->nChannels;
    return true;
}
bool AudioCapturer::_InitAudioClient(IAudioClient* audioClient,
    WAVEFORMATEXTENSIBLE* formatex)
{
    AUDCLNT_SHAREMODE shareMode = AUDCLNT_SHAREMODE_SHARED; // share Audio Engine with other applications
    DWORD streamFlags = _deviceType == Microphone ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK;
    streamFlags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;      // A channel matrixer and a sample
                                                            // rate converter are inserted
    streamFlags |= AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY; // a sample rate converter
                                                            // with better quality than
                                                            // the default conversion but
                                                            // with a higher performance
                                                            // cost is used
    REFERENCE_TIME hnsBufferDuration = 0;
    WAVEFORMATEX* format = &formatex->Format;
    format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format->nBlockAlign = (format->wBitsPerSample >> 3) * format->nChannels;
    format->nAvgBytesPerSec = format->nBlockAlign * format->nSamplesPerSec;
    format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    formatex->Samples.wValidBitsPerSample = format->wBitsPerSample;
    formatex->dwChannelMask = format->nChannels == 1 ? KSAUDIO_SPEAKER_MONO : KSAUDIO_SPEAKER_STEREO;
    formatex->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    __CheckBool(SUCCEEDED(audioClient->Initialize(shareMode, streamFlags, hnsBufferDuration, 0,
        format, nullptr)));
    return true;
}

bool AudioCapturer::_CreateAudioCaptureClient(IAudioClient* audioClient,
    IAudioCaptureClient** audioCaptureClient)
{
    __CheckBool(SUCCEEDED(audioClient->GetService(IID_PPV_ARGS(audioCaptureClient))));
    return true;
}

bool AudioCapturer::_ThreadRun(IAudioClient* audio_client,
    IAudioCaptureClient* audio_capture_client)
{
    UINT32 num_success = 0;
    BYTE* p_audio_data = nullptr;
    UINT32 num_frames_to_read = 0;
    DWORD dw_flag = 0;
    UINT32 num_frames_in_next_packet = 0;
    audio_client->Start();
    while (_loopFlag) {
        SleepMs(5);
        while (true) {
            __CheckBool(SUCCEEDED(audio_capture_client->GetNextPacketSize(&num_frames_in_next_packet)));
            if (num_frames_in_next_packet == 0) {
                break;
            }

            __CheckBool(SUCCEEDED(audio_capture_client->GetBuffer(&p_audio_data, &num_frames_to_read,
                &dw_flag, nullptr, nullptr)));

            size_t size = (_formatex.Format.wBitsPerSample >> 3) * _formatex.Format.nChannels * num_frames_to_read;
            _callback(p_audio_data, size, _userInfo);
            __CheckBool(SUCCEEDED(audio_capture_client->ReleaseBuffer(num_frames_to_read)));
        }
    }

    audio_client->Stop();
    return true;
}
