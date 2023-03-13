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
class CircleBuffer {
public:
    void SetSize(int size);
    bool Pop(uint8_t* data, int length);
    void Push(uint8_t* data, int length);

private:
    std::vector<uint8_t> _buffer;
    int _front = 0;
    int _back = 0;
};

class Resampler {
public:
    bool Open(int inChannelNums, int inSampleRate, AVSampleFormat inFmt,
        int outChannelNums, int outSampleRate, AVSampleFormat outFmt, int outNbSample);
    AVFrame* Convert(uint8_t* data, int size, bool isWriteToFile = false);
    void Close();

private:
    AVFrame* _fromFrame = nullptr;
    AVFrame* _swrFrame = nullptr;
    AVFrame* _toFrame = nullptr;
    SwrContext* _swrCtx = nullptr;
    CircleBuffer _fromBuffer;
    CircleBuffer _toBuffer;
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
        std::queue<Frame<MediaType::AUDIO>> frameQueue;
        float meanVolume = 0;
        float scale = 1;
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
    bool Init(int outputFrameSize = 1024);
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
};

#endif // AUDIOMIXER_H
