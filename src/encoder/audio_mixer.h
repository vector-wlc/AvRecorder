/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-17 19:33:07
 * @Description:
 */
#ifndef __AUDIO_MIXER_H__
#define __AUDIO_MIXER_H__

#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#define __PCM1_FRAME_SIZE (4096 * 2)
#define __PCM2_FRAME_SIZE (4096)
#define __PCM_OUT_FRAME_SIZE (40000)

class AudioMixer {
public:
    struct AudioInfo {
        AudioInfo() { filterCtx = nullptr; }
        AVFilterContext* filterCtx;
        uint32_t sampleRate;
        uint32_t channels;
        uint32_t bitsPerSample;
        AVSampleFormat format;
        std::string name;
    };
    AudioMixer();
    virtual ~AudioMixer();
    // 添加音频输入通道
    bool AddAudioInput(uint32_t index, uint32_t sampleRate, uint32_t channels,
        uint32_t bitsPerSample, AVSampleFormat format);
    // 添加音频输出通道
    bool AddAudioOutput(const uint32_t sampleRate, const uint32_t channels,
        const uint32_t bitsPerSample, const AVSampleFormat format);
    bool AddFrame(uint32_t index, uint8_t* inBuf, uint32_t size);
    bool Init(const char* duration = "longest", int outputFrameSize = 1024);
    AVFrame* GetFrame();
    bool Close();
    const AudioInfo* GetInputInfo(uint32_t index) const;

private:
    AVFilterGraph* _filterGraph;
    bool _inited = false;
    std::mutex _mutex;
    // 输入
    std::unordered_map<uint32_t, AudioInfo> _audioInputInfos;
    // 转换格式
    AudioInfo _audioOutputInfo;
    // 输出
    AudioInfo _audioSinkInfo;
    // 混音
    AudioInfo _audioMixInfo;
    AVFrame* _outputFrame = nullptr;
    int _Init(const char* duration = "longest");
    void _AdjustVolume(uint8_t* buf, int len, double inputMultiple);
    int _pts = 0;
};

#endif // AUDIOMIXER_H
