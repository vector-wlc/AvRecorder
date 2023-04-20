/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-17 19:33:07
 * @Description:
 */
#include "audio_mixer.h"
#include "basic/basic.h"
#include <iostream>
#include <thread>
#include <windows.h>
#include "basic/frame.h"

AVSampleFormat BitsToFmt(int bits)
{
    switch (bits) {
    case 8:
        return AV_SAMPLE_FMT_U8;
    case 16:
        return AV_SAMPLE_FMT_S16;
    case 32:
        return AV_SAMPLE_FMT_S32;
    case 64:
        return AV_SAMPLE_FMT_S64;
    default:
        return AV_SAMPLE_FMT_FLT;
    }
}

int FmtToBits(AVSampleFormat fmt)
{
    switch (fmt) {
    case AV_SAMPLE_FMT_U8:
        return 8;
    case AV_SAMPLE_FMT_S16:
        return 16;
    case AV_SAMPLE_FMT_S32:
        return 32;
    case AV_SAMPLE_FMT_S64:
        return 64;
    default:
        return 32;
    }
}

int SizeToNbSamples(int size, int bitsPerSample, int nbChannels)
{
    return (size << 3) / bitsPerSample / nbChannels;
}

int NbSamplesToSize(int nbSamples, int bitsPerSample, int nbChannels)
{
    return (nbSamples * bitsPerSample * nbChannels) >> 3;
}

bool FrameQueue::Init(int channelNums, int sampleRate, AVSampleFormat fmt, int nbSamples)
{
    _front = 0;
    _sampleRate = sampleRate;
    _fmt = fmt;
    _nbSamples = nbSamples;
    _usedLinesize = nbSamples * channelNums * (fmt == AV_SAMPLE_FMT_S16 ? 2 : 4);
    av_channel_layout_default(&_layout, channelNums);
    _queue.emplace(fmt, &_layout, sampleRate, nbSamples);
    return true;
}

Frame<MediaType::AUDIO> FrameQueue::Pop()
{
    if (_queue.size() > 1) {
        auto frame = std::move(_queue.front());
        _queue.pop();
        return frame;
    }
    return Frame<MediaType::AUDIO>();
}

void FrameQueue::Push(uint8_t* data, int length)
{
    if (length > _usedLinesize) { // 递归调用
        Push(data, length / 2);
        Push(data + length / 2, length / 2 + length % 2);
        return;
    }
    auto&& frame = _queue.back().frame;
    int secondLength = _front + length - _usedLinesize;
    if (secondLength <= 0) { // 第一段缓存是够用的
        memcpy(frame->data[0] + _front, data, length);
        _front += length;
        return;
    }
    // 第一段缓存不够用
    int firstLength = length - secondLength;
    if (firstLength > 0) {
        memcpy(frame->data[0] + _front, data, firstLength);
    }
    // 载入一段新缓存
    _queue.emplace(_fmt, &_layout, _sampleRate, _nbSamples);
    memcpy(_queue.back().frame->data[0], data + firstLength, secondLength);
    _front = secondLength;
}

bool Resampler::Open(int inChannelNums, int inSampleRate, AVSampleFormat inFmt,
    int outChannelNums, int outSampleRate, AVSampleFormat outFmt, int outNbSample)
{
    Close();
    __CheckBool(_swrCtx = swr_alloc());

    AVChannelLayout tmpLayout;
    av_channel_layout_default(&tmpLayout, inChannelNums);
    av_opt_set_chlayout(_swrCtx, "in_chlayout", &tmpLayout, 0);
    av_opt_set_int(_swrCtx, "in_sample_rate", inSampleRate, 0);
    av_opt_set_sample_fmt(_swrCtx, "in_sample_fmt", inFmt, 0);
    __CheckBool(_fromQueue.Init(inChannelNums, inSampleRate, inFmt, inSampleRate / 100 * 2));

    av_channel_layout_default(&tmpLayout, outChannelNums);
    av_opt_set_chlayout(_swrCtx, "out_chlayout", &tmpLayout, 0);
    av_opt_set_int(_swrCtx, "out_sample_rate", outSampleRate, 0);
    av_opt_set_sample_fmt(_swrCtx, "out_sample_fmt", outFmt, 0);
    if (swr_init(_swrCtx) < 0) {
        Close();
        __DebugPrint("swr_init(_swrCtx) failed\n");
        return false;
    }
    __CheckBool(_toQueue.Init(outChannelNums, outSampleRate, outFmt, outNbSample));
    __CheckBool(_swrFrame = Frame<MediaType::AUDIO>::Alloc(outFmt, &tmpLayout, outSampleRate, outSampleRate / 100 * 2));

    return true;
}

void Resampler::Close()
{
    Free(_swrCtx, [this] { swr_free(&_swrCtx); });
    Free(_swrFrame, [this] { av_frame_free(&_swrFrame); });
}

bool Resampler::Convert(uint8_t* data, int size)
{
    std::vector<Frame<MediaType::AUDIO>> ret;
    if (data == nullptr) {
        return false;
    }
    _fromQueue.Push(data, size);
    for (; true;) { // 转换
        auto frame = _fromQueue.Pop();
        if (frame.frame == nullptr) {
            break;
        }
        __CheckNullptr(swr_convert(_swrCtx, _swrFrame->data, _swrFrame->nb_samples, //
            (const uint8_t**)frame.frame->data, frame.frame->nb_samples));
        _toQueue.Push(_swrFrame->data[0], _swrFrame->linesize[0]);
    }
    return true;
}

AVFrame* AudioMixer::Convert(uint32_t index, uint8_t* inBuf, uint32_t size)
{
    std::lock_guard<std::mutex> locker(_mutex);
    auto iter = _audioInputInfos.find(index);
    __CheckNullptr(iter != _audioInputInfos.end());
    __CheckNullptr(iter->second.resampler->Convert(inBuf, size));
    return _AdjustVolume() ? _outputFrame : nullptr;
}

bool AudioMixer::_AdjustVolume()
{
    // 检测所有流之间是不是相差太大了以及缓存的数据是不是太多了
    // 如果缓存的数据太多，直接将所有的队列删除同样的数据
    // 如果两个流直接数据相差太大，将多的那个减到和少的那个一样
    constexpr int MAX_DIFF = 10;
    constexpr int MAX_BUF_SIZE = 20;
    int minSize = INT_MAX;
    int maxSize = INT_MIN;
    FrameQueue* maxQueue = nullptr;
#undef min
    for (auto&& iter : _audioInputInfos) {
        auto&& queue = iter.second.resampler->GetQueue();
        if (queue.IsEmpty()) {
            return false;
        }
        minSize = std::min(minSize, (int)queue.GetSize());
        if (maxSize < (int)queue.GetSize()) {
            maxSize = queue.GetSize();
            maxQueue = &queue;
        }
    }

    if (maxSize - minSize > MAX_DIFF) {
        __DebugPrint("Clear MAX_DIFF");
        for (int i = 0; i < maxSize - minSize; ++i) {
            maxQueue->Pop();
        }
    }

    for (auto iter = _audioInputInfos.begin(); iter != _audioInputInfos.end(); ++iter) {
        auto&& frameQueue = iter->second.resampler->GetQueue();
        if (minSize > MAX_BUF_SIZE) {
            __DebugPrint("Clear MAX_BUF_SIZE");
            for (int i = 0; i < minSize - 2; ++i) {
                frameQueue.Pop();
            }
        }
        auto frame = frameQueue.Pop();
        auto scale = iter->second.scale;
        auto writeStream = (float*)(_outputFrame->data[0]);
        auto readStream = (float*)(frame.frame->data[0]);
        iter->second.volume = readStream[0] * scale;

        if (iter == _audioInputInfos.begin()) {
            if (std::abs(scale - 1) < 0.01) { // 这种情况可以直接使用 memcpy 而不是下面那种低效率的逐个赋值
                memcpy(writeStream, readStream, _outputFrame->linesize[0]);
                continue;
            }
            // 要进行 scale, 只能逐个赋值
            // 所以这里要清零
            memset(writeStream, 0, _outputFrame->linesize[0]);
        }
        // 逐个计算赋值
        for (int idx = 0; idx < _outputFrame->nb_samples; ++idx) {
            writeStream[idx] += readStream[idx] * scale;
            if (writeStream[idx] > 0.99) {
                writeStream[idx] = 0.99;
            }
        }
    }
    return true;
}

AudioMixer::AudioMixer()
    : _inited(false)
{
}

AudioMixer::~AudioMixer()
{
    // delete out_buf;
    if (_inited) {
        Close();
    }
}

bool AudioMixer::AddAudioInput(uint32_t index, uint32_t sampleRate,
    uint32_t channels, uint32_t bitsPerSample,
    AVSampleFormat format)
{
    std::lock_guard<std::mutex> locker(_mutex);
    __CheckBool(!_inited);
    // 根据index保存是否已经存在
    __CheckBool(_audioInputInfos.find(index) == _audioInputInfos.end());

    auto& filterInfo = _audioInputInfos[index];
    // 设置音频相关参数
    filterInfo.sampleRate = sampleRate;
    filterInfo.channels = channels;
    filterInfo.bitsPerSample = bitsPerSample;
    filterInfo.format = format;
    filterInfo.name = std::string("input") + std::to_string(index);
    return true;
}

bool AudioMixer::AddAudioOutput(const uint32_t sampleRate,
    const uint32_t channels,
    const uint32_t bitsPerSample,
    const AVSampleFormat format)
{
    std::lock_guard<std::mutex> locker(_mutex);
    __CheckBool(!_inited);
    // 设置音频相关参数
    _audioOutputInfo.sampleRate = sampleRate;
    _audioOutputInfo.channels = channels;
    _audioOutputInfo.bitsPerSample = bitsPerSample;
    _audioOutputInfo.format = format;
    _audioOutputInfo.name = "output";
    return true;
}

bool AudioMixer::SetOutFrameSize(int outFrameSize)
{
    if (_outFrameSize == outFrameSize) {
        return true;
    }
    _outFrameSize = outFrameSize;
    for (auto&& filterInfoPair : _audioInputInfos) {
        auto&& filterInfo = filterInfoPair.second;
        filterInfo.resampler = std::make_unique<Resampler>();
        __CheckBool(filterInfo.resampler->Open(filterInfo.channels, filterInfo.sampleRate, filterInfo.format,
            _audioOutputInfo.channels, _audioOutputInfo.sampleRate, _audioOutputInfo.format, outFrameSize));
    }
    AVChannelLayout tmpLayout;
    av_channel_layout_default(&tmpLayout, _audioOutputInfo.channels);
    Free(_outputFrame, [this] { av_frame_free(&_outputFrame); });
    __CheckBool(_outputFrame = Frame<MediaType::AUDIO>::Alloc(_audioOutputInfo.format, &tmpLayout, _audioOutputInfo.sampleRate, outFrameSize));
    _inited = true;
    return true;
}

bool AudioMixer::Close()
{
    if (!_inited) {
        return true;
    }
    _inited = false;
    std::lock_guard<std::mutex> locker(_mutex);
    _audioInputInfos.clear();
    Free(_outputFrame, [this] { av_frame_free(&_outputFrame); });
    _outFrameSize = 0;
    return true;
}

AudioMixer::AudioInfo* AudioMixer::GetInputInfo(uint32_t index)
{
    auto iter = _audioInputInfos.find(index);
    return iter == _audioInputInfos.end() ? nullptr : &(iter->second);
}
