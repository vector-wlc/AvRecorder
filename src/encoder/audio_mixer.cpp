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

AudioMixer::AudioMixer()
    : _inited(false)
    , _filterGraph(nullptr)
{
    _audioMixInfo.name = "amix";  // 混音
    _audioSinkInfo.name = "sink"; // 输出
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

bool AudioMixer::Init(const char* duration, int outputFrameSize)
{
    _pts = 0;
    std::lock_guard<std::mutex> locker(_mutex);
    __CheckBool(!_inited);
    __CheckBool(_audioInputInfos.size() != 0);
    // 用于整个过滤流程的一个封装
    // AVFilterGraph是FFmpeg中用于管理和连接音频过滤器的对象。
    __CheckBool(_filterGraph = avfilter_graph_alloc());

    char args[512] = {0};
    // 混音
    // 函数用于获取一个名为 "amix" 的音频滤镜。它返回一个指向 AVFilter
    // 结构体的指针。
    const AVFilter* amix;
    __CheckBool(amix = avfilter_get_by_name("amix"));
    __CheckBool(_audioMixInfo.filterCtx = avfilter_graph_alloc_filter(_filterGraph, amix, "amix"));
    // inputs=输入流数量
    // duration=决定流的结束(longest最长输入时间,shortest最短,first第一个输入持续的时间)
    // dropout_transition= 输入流结束时,音量重整时间
    snprintf(args, sizeof(args), "inputs=%d:duration=%s:dropout_transition=0",
        _audioInputInfos.size(), duration);
    __CheckBool(avfilter_init_str(_audioMixInfo.filterCtx, args) == 0);

    // 输出
    const AVFilter* abuffersink = nullptr;
    __CheckBool(abuffersink = avfilter_get_by_name("abuffersink"));
    __CheckBool(_audioSinkInfo.filterCtx = avfilter_graph_alloc_filter(_filterGraph, abuffersink, "sink"));
    __CheckBool(avfilter_init_str(_audioSinkInfo.filterCtx, nullptr) >= 0);

    // 输入
    int cnt = 0;
    for (auto& iter : _audioInputInfos) {
        AVChannelLayout tmpLayout;
        av_channel_layout_default(&tmpLayout, iter.second.channels);
        const AVFilter* abuffer = nullptr;
        __CheckBool(abuffer = avfilter_get_by_name("abuffer"));
        snprintf(args, sizeof(args),
            "sample_rate=%d:sample_fmt=%s:channel_layout=0x%I64x",
            iter.second.sampleRate,
            av_get_sample_fmt_name(iter.second.format),
            tmpLayout.u.mask);

        __CheckBool(iter.second.filterCtx = avfilter_graph_alloc_filter(
                        _filterGraph, abuffer, _audioOutputInfo.name.c_str()));
        __CheckBool(avfilter_init_str(iter.second.filterCtx, args) >= 0);
        // _audioInputInfos[index] -> audio_min_info_ptr[index]
        __CheckBool(avfilter_link(iter.second.filterCtx, 0, _audioMixInfo.filterCtx, cnt) >= 0);
        ++cnt;
    }

    // 转换格式
    const AVFilter* aformat = nullptr;
    __CheckBool(aformat = avfilter_get_by_name("aformat"));
    AVChannelLayout tmpLayout;
    av_channel_layout_default(&tmpLayout, _audioOutputInfo.channels);
    snprintf(args, sizeof(args),
        "sample_rates=%d:sample_fmts=%s:channel_layouts=0x%I64x",
        _audioOutputInfo.sampleRate,
        av_get_sample_fmt_name(_audioOutputInfo.format),
        tmpLayout.u.mask);
    __CheckBool(_audioOutputInfo.filterCtx = avfilter_graph_alloc_filter(_filterGraph, aformat, "aformat"));
    __CheckBool(avfilter_init_str(_audioOutputInfo.filterCtx, args) >= 0);
    // _audioMixInfo -> _audioOutputInfo
    __CheckBool(avfilter_link(_audioMixInfo.filterCtx, 0, _audioOutputInfo.filterCtx, 0) >= 0);
    // _audioOutputInfo -> _audioSinkInfo
    __CheckBool(avfilter_link(_audioOutputInfo.filterCtx, 0, _audioSinkInfo.filterCtx, 0) >= 0);
    av_buffersink_set_frame_size(_audioSinkInfo.filterCtx, outputFrameSize);
    __CheckBool(avfilter_graph_config(_filterGraph, NULL) >= 0);
    __CheckBool(_outputFrame = av_frame_alloc());
    _inited = true;
    return true;
}

bool AudioMixer::Close()
{
    _pts = 0;
    if (!_inited) {
        return true;
    }
    std::lock_guard<std::mutex> locker(_mutex);
    // 释放输入
    for (auto iter : _audioInputInfos) {
        Free(iter.second.filterCtx, [&iter] { avfilter_free(iter.second.filterCtx); });
    }

    _audioInputInfos.clear();
    // 释放格式转换
    Free(_audioOutputInfo.filterCtx, [this] { avfilter_free(_audioOutputInfo.filterCtx); });
    // 释放混音
    Free(_audioMixInfo.filterCtx, [this] { avfilter_free(_audioMixInfo.filterCtx); });
    // 释放输出
    Free(_audioSinkInfo.filterCtx, [this] { avfilter_free(_audioSinkInfo.filterCtx); });
    Free(_filterGraph, [this] { avfilter_graph_free(&_filterGraph); });
    _inited = false;
    Free(_outputFrame, [this] { av_frame_free(&_outputFrame); });
    return true;
}

const AudioMixer::AudioInfo* AudioMixer::GetInputInfo(uint32_t index) const
{
    auto iter = _audioInputInfos.find(index);
    return iter == _audioInputInfos.end() ? nullptr : &(iter->second);
}

// 添加一帧，根据index添加相应的输入流
bool AudioMixer::AddFrame(uint32_t index, uint8_t* inBuf, uint32_t size)
{
    std::lock_guard<std::mutex> locker(_mutex);
    __CheckBool(_inited);
    auto iter = _audioInputInfos.find(index);
    __CheckBool(iter != _audioInputInfos.end());

    if (inBuf && size > 0) {
        std::shared_ptr<AVFrame> frame(av_frame_alloc(), [](AVFrame* ptr) { av_frame_free(&ptr); });
        // 设置音频参数
        frame->sample_rate = iter->second.sampleRate;
        frame->format = iter->second.format;
        av_channel_layout_default(&frame->ch_layout, iter->second.channels);
        frame->nb_samples = (size << 3) / iter->second.bitsPerSample / iter->second.channels;
        // 根据音频参数分配空间
        __CheckBool(av_frame_get_buffer(frame.get(), 0) >= 0);
        memcpy(frame->data[0], inBuf, size);
        __CheckBool(av_buffersrc_add_frame(iter->second.filterCtx, frame.get()) >= 0);
        av_frame_unref(frame.get());
    } else {
        // 冲刷
        __CheckBool(av_buffersrc_add_frame(iter->second.filterCtx, nullptr) >= 0);
    }
    return true;
}

// 获取一帧
AVFrame* AudioMixer::GetFrame()
{
    std::lock_guard<std::mutex> locker(_mutex);
    __CheckNullptr(_inited);
    // 获取输出帧
    // 这里得到错误是正常情况，不需要进行错误打印
    if (av_buffersink_get_frame(_audioSinkInfo.filterCtx, _outputFrame) < 0) {
        return nullptr;
    }
    _outputFrame->pts = _pts;
    _pts += _outputFrame->nb_samples;
    return _outputFrame;
}

void AudioMixer::_AdjustVolume(uint8_t* buf, int len, double inputMultiple)
{
    for (int i = 0; i < len; i += 2) {
        short sample = *(short*)(buf + i);
        sample = sample * inputMultiple;
        if (sample > 32767) {
            sample = 32767;
        }
        *(short*)(buf + i) = sample;
    }
}
