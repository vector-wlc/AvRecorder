/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-19 15:34:07
 * @Description:
 */

#ifndef __AUDIO_CAPTURER_H__
#define __AUDIO_CAPTURER_H__

#include <audioclient.h>
#include <combaseapi.h>
#include <mmdeviceapi.h>

#include <memory>
#include <thread>

class AudioCapturer {
public:
    enum Type {
        Microphone,
        Speaker
    };
    using CallBack = void (*)(void* data, size_t size, void* userInfo);

    bool Init(Type deviceType, CallBack callback, void* userInfo = nullptr);
    bool Start();
    const WAVEFORMATEX& GetFormat() const { return _formatex.Format; }

    void Stop();

private:
    bool _isInit = false;
    CallBack _callback;
    Type _deviceType;
    IMMDeviceEnumerator* _pDeviceEnumerator = nullptr;
    IMMDevice* _pDevice = nullptr;
    IAudioClient* _pAudioClient = nullptr;
    IAudioCaptureClient* _pAudioCaptureClient = nullptr;
    std::thread* _captureThread = nullptr;
    bool _loopFlag = false;
    WAVEFORMATEXTENSIBLE _formatex;
    void* _userInfo = nullptr;

    bool _CreateDeviceEnumerator(IMMDeviceEnumerator** enumerator);
    bool _CreateDevice(IMMDeviceEnumerator* enumerator, IMMDevice** device);
    bool _CreateAudioClient(IMMDevice* device, IAudioClient** audioClient);
    bool _IsFormatSupported(IAudioClient* audioClient);
    bool _GetPreferFormat(IAudioClient* audioClient,
        WAVEFORMATEXTENSIBLE* formatex);
    bool _InitAudioClient(IAudioClient* audioClient,
        WAVEFORMATEXTENSIBLE* formatex);
    bool _CreateAudioCaptureClient(IAudioClient* audioClient,
        IAudioCaptureClient** audioCaptureClient);
    bool _ThreadRun(IAudioClient* audio_client,
        IAudioCaptureClient* audio_capture_client);
};

#endif