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
#include <libswresample/swresample.h>
}

#include "basic/frame.h"

#define __PCM1_FRAME_SIZE (4096 * 2)
#define __PCM2_FRAME_SIZE (4096)
#define __PCM_OUT_FRAME_SIZE (40000)

// 循环缓存空间
class FrameQueue {
public:
    bool Init(int channelNums, int sampleRate, AVSampleFormat fmt, int nbSamples);
    Frame<MediaType::AUDIO> Pop();
    void Push(uint8_t* data, int length);
    bool IsEmpty() const { return _queue.size() < 2; }
    auto GetSize() const { return _queue.size(); }

private:
    int _front = 0;
    AVChannelLayout _layout;
    int _sampleRate;
    int _nbSamples;
    int _usedLinesize;
    AVSampleFormat _fmt;
    std::queue<Frame<MediaType::AUDIO>> _queue;
};

class Resampler {
public:
    bool Open(int inChannelNums, int inSampleRate, AVSampleFormat inFmt,
        int outChannelNums, int outSampleRate, AVSampleFormat outFmt, int outNbSample);
    bool Convert(uint8_t* data, int size);
    void Close();
    FrameQueue& GetQueue() { return _toQueue; }
    ~Resampler() { Close(); }

private:
    AVFrame* _swrFrame = nullptr;
    SwrContext* _swrCtx = nullptr;
    FrameQueue _fromQueue;
    FrameQueue _toQueue;
};

class AudioMixer {
public:
    struct AudioInfo {
        uint32_t sampleRate;
        uint32_t channels;
        uint32_t bitsPerSample;
        AVSampleFormat format;
        std::string name;
        std::unique_ptr<Resampler> resampler;
        float volume = 0;
        float scale = 1;
        int callTime = 0;
    };
    AudioMixer();
    virtual ~AudioMixer();
    // 添加音频输入通道
    bool AddAudioInput(uint32_t index, uint32_t sampleRate, uint32_t channels,
        uint32_t bitsPerSample, AVSampleFormat format);
    // 添加音频输出通道
    bool AddAudioOutput(const uint32_t sampleRate, const uint32_t channels,
        const uint32_t bitsPerSample, const AVSampleFormat format);
    AVFrame* Convert(uint32_t index, uint8_t* inBuf, uint32_t size);
    bool SetOutFrameSize(int outputFrameSize = 1024);
    int GetOutFrameSize() const { return _outFrameSize; };
    bool Close();
    AudioInfo* GetInputInfo(uint32_t index);

private:
    bool _inited = false;
    std::mutex _mutex;
    // 输入
    std::unordered_map<uint32_t, AudioInfo> _audioInputInfos;
    // 转换格式
    AudioInfo _audioOutputInfo;
    AVFrame* _outputFrame = nullptr;
    bool _AdjustVolume();
    int _outFrameSize = 0;
};

#endif // AUDIOMIXER_H
