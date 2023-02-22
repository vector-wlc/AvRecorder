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
        float scaleRate = 1;  // 音频幅度调整参数
        float meanVolume = 0; // 每个音频的平均音量，用于渲染音量 [0, 1]
        bool isUsable = false;
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
    const Info* GetCaptureInfo(int mixIndex) const
    {
        return mixIndex >= 0 && mixIndex < _infos.size() ? &_infos[mixIndex] : nullptr;
    }
    void SetVolumeScale(float scale, int mixIndex)
    {
        if (mixIndex >= 0 && mixIndex < _infos.size()) {
            _infos[mixIndex].scaleRate = scale;
        }
    }

private:
    AudioCapturer _micCapturer;
    AudioCapturer _speakerCapturer;
    AudioMixer _mixer;
    std::vector<Info> _infos;
    bool _isRecord = false;
    int _streamIndex;
    Encoder<MediaType::AUDIO>::Param _param;

    static void _Callback(void* data, size_t size, void* userInfo);
    static float _AdjustVolume(uint8_t* buf, int step, int len, float inputMultiple);

    template <typename T>
    static float _AdjustVolumeT(uint8_t* buf, int len, float inputMultiple)
    {
        constexpr int32_t maxVal = T(~(1 << (sizeof(T) * 8 - 1)));
        if (std::abs(inputMultiple - 1) > 0.01) { // 与 1 相差较大时才会进行音量调节
            for (int i = 0; i < len; i += sizeof(T)) {
                int32_t sample = *((T*)(buf + i));
                sample *= inputMultiple;
                if (sample > maxVal) {
                    sample = maxVal;
                }
                if (sample < -maxVal) {
                    sample = -maxVal;
                }
                *((T*)(buf + i)) = sample;
            }
        }
        return std::abs(*(T*)buf * 1.0 / maxVal); // 直接取第一个元素的音量当作代表
    }

    AVSampleFormat _GetAVSampleFormat(int wBitsPerSample)
    {
        return wBitsPerSample == 16 ? AV_SAMPLE_FMT_S16 : AV_SAMPLE_FMT_S32;
    }
};

#endif