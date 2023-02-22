/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-17 19:33:07
 * @Description:
 */
#include "audio_recorder.h"

bool AudioRecorder::Open(
    const std::vector<AudioCapturer::Type>& deviceTypes,
    Encoder<MediaType::AUDIO>::Param& param,
    const uint32_t sampleRate,
    const uint32_t channels,
    const uint32_t bitsPerSample,
    const AVSampleFormat format)
{
    Close();
    Info mixInfo;
    mixInfo.mixer = &_mixer;
    mixInfo.isRecord = &_isRecord;
    mixInfo.streamIndex = &_streamIndex;

    for (int index = 0; index < deviceTypes.size(); ++index) {
        mixInfo.mixIndex = index;
        _infos.push_back(mixInfo);
    }
    for (int index = 0; index < deviceTypes.size(); ++index) {
        auto&& capturer = deviceTypes[index] == AudioCapturer::Microphone ? _micCapturer : _speakerCapturer;
        if (!capturer.Init(deviceTypes[index], _Callback, &(_infos[index]))) {
            _infos[index].isUsable = false;
            continue;
        }
        _infos[index].isUsable = true;
        auto&& format = capturer.GetFormat();
        __CheckBool(_mixer.AddAudioInput(index, format.nSamplesPerSec, format.nChannels,
            format.wBitsPerSample, _GetAVSampleFormat(format.wBitsPerSample)));
    }
    __CheckBool(_mixer.AddAudioOutput(sampleRate, channels, bitsPerSample, format));
    // 临时打开封装器，为的是获取编码器的一些信息
    AvMuxer muxer;
    __CheckBool(muxer.Open("tmp.mp4"));
    _param = param;
    int streamIndex = -1;
    __CheckBool((streamIndex = muxer.AddAudioStream(param)) != -1);
    __CheckBool(_mixer.Init("longest", muxer.GetCodecCtx(streamIndex)->frame_size));
    muxer.Close();
    for (int index = 0; index < deviceTypes.size(); ++index) {
        if (_infos[index].isUsable) {
            auto&& capturer = deviceTypes[index] == AudioCapturer::Microphone ? _micCapturer : _speakerCapturer;
            __CheckBool(capturer.Start());
        }
    }

    return true;
}

void AudioRecorder::Close()
{
    StopRecord();
    _micCapturer.Stop();
    _speakerCapturer.Stop();
    _mixer.Close();
    _infos.clear();
}

bool AudioRecorder::LoadMuxer(AvMuxer& muxer)
{
    for (auto&& info : _infos) {
        info.muxer = &muxer;
    }
    __CheckBool((_streamIndex = muxer.AddAudioStream(_param)) != -1);
    return true;
}

bool AudioRecorder::StartRecord()
{
    _isRecord = true;
    return true;
}

void AudioRecorder::StopRecord()
{
    _isRecord = false;
}

void AudioRecorder::_Callback(void* data, size_t size, void* userInfo)
{
    auto info = (Info*)userInfo;
    auto inputInfo = info->mixer->GetInputInfo(info->mixIndex);
    if (inputInfo != nullptr) {
        info->meanVolume = _AdjustVolume((uint8_t*)data, inputInfo->bitsPerSample / 8, size, info->scaleRate);
    }
    info->mixer->AddFrame(info->mixIndex, (uint8_t*)data, size);
    auto frame = info->mixer->GetFrame();
    if (frame == nullptr) {
        return;
    }
    if (*(info->isRecord)) {
        __CheckNo(info->streamIndex && *(info->streamIndex) != -1);
        __CheckNo(info->muxer->Write(frame, *(info->streamIndex)));
    }
    av_frame_unref(frame);
}

float AudioRecorder::_AdjustVolume(uint8_t* buf, int step, int len, float inputMultiple)
{
    switch (step) {
    case 1:
        return _AdjustVolumeT<int8_t>(buf, len, inputMultiple);
    case 2:
        return _AdjustVolumeT<int16_t>(buf, len, inputMultiple);
    case 4:
        return _AdjustVolumeT<int32_t>(buf, len, inputMultiple);
    default:
        printf("undefined format\n");
        return 0;
    }
}
