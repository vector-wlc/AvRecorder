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

void CircleBuffer::SetSize(int size)
{
    _buffer.resize(size);
    _front = 0;
    _back = 0;
}

bool CircleBuffer::Pop(uint8_t* data, int length)
{
    auto bufSize = _buffer.size();
    // 需要判断当前缓存的数据长度是否够用
    // 不够用返回 nullptr
    int dataLength = _front >= _back ? _front - _back : _front - _back + bufSize;
    if (dataLength < length) {
        return false;
    }
    int secondSize = _back + length - bufSize;
    if (secondSize <= 0) { // 缓存没有发生循环
        memcpy(data, _buffer.data() + _back, length);
        _back += length;
        return true;
    }
#undef max
    // 缓存发生了循环
    int firstSize = std::max(length - secondSize, 0);
    if (firstSize > 0) {
        memcpy(data, _buffer.data() + _back, firstSize);
    }
    memcpy(data + firstSize, _buffer.data(), secondSize);
    _back = secondSize;
    return true;
}

void CircleBuffer::Push(uint8_t* data, int length)
{
    auto bufSize = _buffer.size();
    int dataLength = _front >= _back ? _front - _back : _front - _back + bufSize;
    if (dataLength + length > bufSize) { // 需要申请内存了
        // 将原来的内存复制到新申请的内存上面
        decltype(_buffer) newBuffer((dataLength + length) * 2);
        __CheckNo(Pop(newBuffer.data(), dataLength)); // 弹出之前的缓存
        _buffer.swap(newBuffer);
        _front = dataLength;
        _back = 0;
    }
    int secondSize = length - (_buffer.size() - _front);
    if (secondSize <= 0) { // 不需要循环拷贝
        memcpy(_buffer.data() + _front, data, length);
        _front += length;
        return;
    }

    // 需要循环拷贝
    memcpy(_buffer.data() + _front, data, length - secondSize);
    memcpy(_buffer.data(), data, secondSize);
    _front = secondSize;
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
    __CheckBool(_fromFrame = Frame<MediaType::AUDIO>::Alloc(inFmt, &tmpLayout, inSampleRate, outSampleRate / 100));

    av_channel_layout_default(&tmpLayout, outChannelNums);
    av_opt_set_chlayout(_swrCtx, "out_chlayout", &tmpLayout, 0);
    av_opt_set_int(_swrCtx, "out_sample_rate", outSampleRate, 0);
    av_opt_set_sample_fmt(_swrCtx, "out_sample_fmt", outFmt, 0);
    if (swr_init(_swrCtx) < 0) {
        Close();
        __DebugPrint("swr_init(_swrCtx) failed\n");
        return false;
    }
    __CheckBool(_swrFrame = Frame<MediaType::AUDIO>::Alloc(outFmt, &tmpLayout, outSampleRate, inSampleRate / 100));
    __CheckBool(_toFrame = Frame<MediaType::AUDIO>::Alloc(outFmt, &tmpLayout, outSampleRate, outNbSample));

    _fromBuffer.SetSize(_fromFrame->linesize[0] * 2);
    _toBuffer.SetSize(_toFrame->linesize[0] * 2);
    return true;
}

AVFrame* Resampler::Convert(uint8_t* data, int size, bool isWriteToFile)
{
    // 第一：此函数需要等待接受的数据是 输出频率 / 100 的整倍数
    // 第二：此函数转换完成之后需要等待数据攒够 outNbSample 才能输出
    if (data == nullptr) {
        return nullptr;
    }
    _fromBuffer.Push(data, size);
    for (; true;) { // 转换
        if (!_fromBuffer.Pop(_fromFrame->data[0], _fromFrame->linesize[0])) {
            break;
        }
        __DebugPrint("%d %d", _fromFrame->linesize[0], memcmp(data, _fromFrame->data[0], _fromFrame->linesize[0]));
        if (isWriteToFile) {
            auto fPtr = fopen("test.pcm", "ab");
            fwrite(_fromFrame->data[0], _fromFrame->linesize[0], 1, fPtr);
            fclose(fPtr);
        }
        __CheckNullptr(swr_convert(_swrCtx, _swrFrame->data, _swrFrame->nb_samples,
            (const uint8_t**)_fromFrame->data, _fromFrame->nb_samples));
        _toBuffer.Push(_swrFrame->data[0], _swrFrame->linesize[0]);
    }
    if (!_toBuffer.Pop(_toFrame->data[0], _toFrame->linesize[0])) {
        return nullptr;
    }
    return _toFrame;
}

void Resampler::Close()
{
    Free(_swrCtx, [this] { swr_free(&_swrCtx); });
    Free(_toFrame, [this] { av_frame_free(&_toFrame); });
    Free(_swrFrame, [this] { av_frame_free(&_swrFrame); });
    Free(_fromFrame, [this] { av_frame_free(&_fromFrame); });
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

bool AudioMixer::Init(int outputFrameSize)
{
    for (auto&& filterInfoPair : _audioInputInfos) {
        auto&& filterInfo = filterInfoPair.second;
        filterInfo.resampler = std::make_unique<Resampler>();
        __CheckBool(filterInfo.resampler->Open(filterInfo.channels, filterInfo.sampleRate, filterInfo.format,
            _audioOutputInfo.channels, _audioOutputInfo.sampleRate, _audioOutputInfo.format, outputFrameSize));
    }
    AVChannelLayout tmpLayout;
    av_channel_layout_default(&tmpLayout, _audioOutputInfo.channels);
    _inited = true;
    return true;
}

bool AudioMixer::Close()
{
    if (!_inited) {
        return true;
    }
    std::lock_guard<std::mutex> locker(_mutex);
    _audioInputInfos.clear();
    return true;
}

AudioMixer::AudioInfo* AudioMixer::GetInputInfo(uint32_t index)
{
    auto iter = _audioInputInfos.find(index);
    return iter == _audioInputInfos.end() ? nullptr : &(iter->second);
}

// 添加一帧，根据index添加相应的输入流
AVFrame* AudioMixer::Convert(uint32_t index, uint8_t* inBuf, uint32_t size)
{
    std::lock_guard<std::mutex> locker(_mutex);
    auto iter = _audioInputInfos.find(index);
    __CheckNullptr(iter != _audioInputInfos.end());
    auto frame = iter->second.resampler->Convert(inBuf, size, index == 1);
    if (frame != nullptr) {
        iter->second.frameQueue.emplace(frame);
    }
    return _AdjustVolume() ? _outputFrame : nullptr;
}

bool AudioMixer::_AdjustVolume()
{
    for (auto&& iter : _audioInputInfos) {
        if (iter.second.frameQueue.empty()) {
            return false;
        }
    }

    for (auto iter = _audioInputInfos.begin(); iter != _audioInputInfos.end(); ++iter) {
        auto frame = std::move(iter->second.frameQueue.front());
        iter->second.frameQueue.pop();
        iter->second.meanVolume = *(float*)frame.frame->data[0];
        if (iter == _audioInputInfos.begin()) {
            _outputFrame = frame.frame;
            frame.frame = nullptr;
        } else {
            for (int idx = 0; idx < _outputFrame->nb_samples; ++idx) {
                ((float*)(_outputFrame->data[0]))[idx] += ((float*)(frame.frame->data[0]))[idx] * iter->second.scale;
            }
        }
    }
    return true;

    // for (auto&& iter : _audioInputInfos) {
    // }
    // for (int i = 0; i < len; i += 2) {
    //     short sample = *(short*)(buf + i);
    //     sample = sample * inputMultiple;
    //     if (sample > 32767) {
    //         sample = 32767;
    //     }
    //     *(short*)(buf + i) = sample;
    // }
}
