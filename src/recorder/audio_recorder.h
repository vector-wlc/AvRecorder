/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-19 20:01:09
 * @Description:
 */
#ifndef __AUDIO_RECORDER_H__
#define __AUDIO_RECORDER_H__

#include "capturer/audio_capturer.h"
#include "encoder/audio_mixer.h"
#include "muxer/av_muxer.h"

class AudioRecorder {
public:
    struct Info {
        AudioMixer* mixer = nullptr;
        AvMuxer* muxer = nullptr;
        bool* isRecord = nullptr;
        int mixIndex;
        int* streamIndex = nullptr;
    };

    bool Open(const std::vector<AudioCapturer::Type>& deviceTypes,
        Encoder<MediaType::AUDIO>::Param& param,
        const uint32_t sampleRate = AUDIO_SAMPLE_RATE,
        const uint32_t channels = AUDIO_CHANNEL,
        const uint32_t bitsPerSample = 32,
        const AVSampleFormat format = AUDIO_FMT);
    bool LoadMuxer(AvMuxer& muxer);
    bool StartRecord();
    void StopRecord();
    void Close();
    auto GetCaptureInfo(int mixIndex)
    {
        return _mixer.GetInputInfo(mixIndex);
    }
    void SetVolumeScale(float scale, int mixIndex);

private:
    AudioCapturer _micCapturer;
    AudioCapturer _speakerCapturer;
    AudioMixer _mixer;
    std::vector<Info> _infos;
    bool _isRecord = false;
    int _streamIndex;
    Encoder<MediaType::AUDIO>::Param _param;
    static void _Callback(void* data, size_t size, void* userInfo);
    AVSampleFormat _GetAVSampleFormat(int wBitsPerSample)
    {
        return wBitsPerSample == 16 ? AV_SAMPLE_FMT_S16 : AV_SAMPLE_FMT_S32;
    }
};

#endif