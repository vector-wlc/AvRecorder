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
            continue;
        }
        auto&& format = capturer.GetFormat();
        __CheckBool(_mixer.AddAudioInput(index, format.nSamplesPerSec, format.nChannels,
            format.wBitsPerSample, _GetAVSampleFormat(format.wBitsPerSample)));
    }
    __CheckBool(_mixer.AddAudioOutput(sampleRate, channels, bitsPerSample, format));
    _param = param;
    __CheckBool(_mixer.SetOutFrameSize(1024));

    for (int index = 0; index < deviceTypes.size(); ++index) {
        if (_mixer.GetInputInfo(index) != nullptr) {
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

void AudioRecorder::SetVolumeScale(float scale, int mixIndex)
{
    auto info = _mixer.GetInputInfo(mixIndex);
    if (info != nullptr) {
        info->scale = scale;
    }
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
    auto frame = info->mixer->Convert(info->mixIndex, (uint8_t*)data, size);
    if (frame == nullptr) {
        return;
    }
    if (*(info->isRecord)) {
        __CheckNo(info->streamIndex && *(info->streamIndex) != -1);
        int frameSize = info->muxer->GetCodecCtx(*info->streamIndex)->frame_size;
        if (info->mixer->GetOutFrameSize() != frameSize) {
            __DebugPrint("Change frame size from %d to %d", info->mixer->GetOutFrameSize(), frameSize);
            info->mixer->SetOutFrameSize(frameSize);
            return;
        }
        __CheckNo(info->muxer->Write(frame, *(info->streamIndex)));
    }
}
