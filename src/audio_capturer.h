
#ifndef __AUDIO_CAPTURER_H__
#define __AUDIO_CAPTURER_H__

#include <audioclient.h>
#include <combaseapi.h>
#include <mmdeviceapi.h>

#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdint.h>
#include <string>
#include <thread>

#define SAFE_RELEASE(p)     \
    do {                    \
        if ((p)) {          \
            (p)->Release(); \
            (p) = NULL;     \
        }                   \
    } while (0)

#define logd() std::cout << "[Lay] "

#define DEFAULT_SAMPLE_RATE 48000        // 默认采样率:48kHz
#define DEFAULT_BITS_PER_SAMPLE 16       // 默认位深:16bit
#define DEFAULT_CHANNELS 1               // 默认音频通道数:1
#define DEFAULT_AUDIO_PACKET_INTERVAL 10 // 默认音频包发送间隔:10ms
#define Process_if_failed(hr) \
    if (FAILED(hr)) {         \
        return false;         \
    }

class AudioCapturer {
public:
    using CallBack = void (*)(void* data, int bitsPerSample, int channels,
        int frames);
    bool Start(bool isMicrophone, CallBack callback)
    {
        HRESULT hr;
        _callback = callback;
        _isMicrophone = isMicrophone;
        hr = _CreateDeviceEnumerator(&_pDeviceEnumerator);
        Process_if_failed(hr);

        hr = _CreateDevice(_pDeviceEnumerator, &_pDevice);
        Process_if_failed(hr);

        hr = _CreateAudioClient(_pDevice, &_pAudioClient);
        Process_if_failed(hr);

        hr = _IsFormatSupported(_pAudioClient);
        if (FAILED(hr)) {
            hr = _GetPreferFormat(_pAudioClient, &_formatex);
            if (FAILED(hr)) {
                Process_if_failed(hr);
            }
        }

        hr = _InitAudioClient(_pAudioClient, &_formatex);
        Process_if_failed(hr);

        hr = _CreateAudioCaptureClient(_pAudioClient, &_pAudioCaptureClient);
        Process_if_failed(hr);

        _loopFlag = true;
        _capture_thread = std::make_unique<std::thread>(
            [this] { _ThreadRun(_pAudioClient, _pAudioCaptureClient); });
        return true;
    }

    WAVEFORMATEX GetFormat() { return _formatex.Format; }

    void Stop()
    {
        _loopFlag = false;
        if (_capture_thread != nullptr) {
            _capture_thread->join();
        }
        SAFE_RELEASE(_pAudioCaptureClient);
        if (_pAudioClient != nullptr) {
            _pAudioClient->Stop();
        }
        SAFE_RELEASE(_pAudioClient);
        SAFE_RELEASE(_pDevice);
        SAFE_RELEASE(_pDeviceEnumerator);
    }

private:
    CallBack _callback;
    bool _isMicrophone = false;
    IMMDeviceEnumerator* _pDeviceEnumerator = nullptr;
    IMMDevice* _pDevice = nullptr;
    IAudioClient* _pAudioClient = nullptr;
    IAudioCaptureClient* _pAudioCaptureClient = nullptr;
    std::unique_ptr<std::thread> _capture_thread = nullptr;
    bool _loopFlag = false;
    WAVEFORMATEXTENSIBLE _formatex;

    HRESULT _CreateDeviceEnumerator(IMMDeviceEnumerator** enumerator)
    {
        auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
            return hr;
        }
        return CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            reinterpret_cast<void**>(enumerator));
    }
    HRESULT _CreateDevice(IMMDeviceEnumerator* enumerator, IMMDevice** device)
    {
        EDataFlow enDataFlow = _isMicrophone ? eCapture : eRender;
        ERole enRole = eConsole;
        return enumerator->GetDefaultAudioEndpoint(enDataFlow, enRole, device);
    }
    HRESULT _CreateAudioClient(IMMDevice* device, IAudioClient** audioClient)
    {
        return device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
            (void**)audioClient);
    }
    HRESULT _IsFormatSupported(IAudioClient* audioClient)
    {
        WAVEFORMATEX* format = &_formatex.Format;
        format->nSamplesPerSec = DEFAULT_SAMPLE_RATE;
        format->wBitsPerSample = DEFAULT_BITS_PER_SAMPLE;
        format->nChannels = DEFAULT_CHANNELS;

        WAVEFORMATEX* closestMatch = nullptr;

        HRESULT hr = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
            format, &closestMatch);
        if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) // 0x88890008
        {
            if (closestMatch != nullptr) // 如果找不到最相近的格式，closestMatch可能为nullptr
            {
                logd() << "Epxected format: "
                       << "sample_rate[" << format->nSamplesPerSec << "] "
                       << "bits_per_sample[" << format->wBitsPerSample << "] "
                       << "channels[" << format->nChannels << "] "
                       << "\n"
                       << "Supported format: "
                       << "sample_rate[" << closestMatch->nSamplesPerSec << "] "
                       << "bits_per_sample[" << closestMatch->wBitsPerSample << "] "
                       << "channels[" << closestMatch->nChannels << "] "
                       << "\n";

                format->nSamplesPerSec = closestMatch->nSamplesPerSec;
                format->wBitsPerSample = closestMatch->wBitsPerSample;
                format->nChannels = closestMatch->nChannels;

                return S_OK;
            }
        }

        return hr;
    }
    HRESULT _GetPreferFormat(IAudioClient* audioClient,
        WAVEFORMATEXTENSIBLE* formatex)
    {
        WAVEFORMATEX* format = nullptr;
        HRESULT hr = audioClient->GetMixFormat(&format);
        if (FAILED(hr)) {
            return hr;
        }

        // logd() << "Prefer format: "
        //	<< "sample_rate[" << format->nSamplesPerSec << "] "
        //	<< "bits_per_sample[" << format->wBitsPerSample << "] "
        //	<< "channels[" << format->nChannels << "] "
        //	<< "\n";

        formatex->Format.nSamplesPerSec = format->nSamplesPerSec;
        formatex->Format.wBitsPerSample = format->wBitsPerSample;
        formatex->Format.nChannels = format->nChannels;

        return hr;
    }
    HRESULT _InitAudioClient(IAudioClient* audioClient,
        WAVEFORMATEXTENSIBLE* formatex)
    {
        AUDCLNT_SHAREMODE shareMode = AUDCLNT_SHAREMODE_SHARED; // share Audio Engine with other applications
        DWORD streamFlags = _isMicrophone ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK;
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

        return audioClient->Initialize(shareMode, streamFlags, hnsBufferDuration, 0,
            format, nullptr);
    }

    HRESULT _CreateAudioCaptureClient(IAudioClient* audioClient,
        IAudioCaptureClient** audioCaptureClient)
    {
        HRESULT hr = audioClient->GetService(IID_PPV_ARGS(audioCaptureClient));
        if (FAILED(hr)) {
            *audioCaptureClient = nullptr;
        }
        return hr;
    }

    void _ThreadRun(IAudioClient* audio_client,
        IAudioCaptureClient* audio_capture_client)
    {
        HRESULT hr = S_OK;
        UINT32 num_success = 0;

        BYTE* p_audio_data = nullptr;
        UINT32 num_frames_to_read = 0;
        DWORD dw_flag = 0;

        UINT32 num_frames_in_next_packet = 0;

        UINT32 num_loop = 0;

        audio_client->Start();

        while (_loopFlag) {
            std::this_thread::sleep_for(std::chrono::milliseconds(0));

            while (true) {
                hr = audio_capture_client->GetNextPacketSize(&num_frames_in_next_packet);
                if (FAILED(hr)) {
                    throw std::exception();
                }
                if (num_frames_in_next_packet == 0) {
                    break;
                }

                hr = audio_capture_client->GetBuffer(&p_audio_data, &num_frames_to_read,
                    &dw_flag, nullptr, nullptr);
                if (FAILED(hr)) {
                    throw std::exception();
                }

                _callback(p_audio_data, _formatex.Format.wBitsPerSample,
                    _formatex.Format.nChannels, num_frames_to_read);

                if (num_success++ % 500 == 0) {
                    std::cout << "Have already cpatured [" << num_success << "] times."
                              << std::endl;
                }

                hr = audio_capture_client->ReleaseBuffer(num_frames_to_read);
                if (FAILED(hr)) {
                    throw std::exception();
                }

                num_loop++;
            }
        }

        audio_client->Stop();
    }
};

#endif