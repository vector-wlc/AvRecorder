/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-19 19:36:28
 * @Description:
 */
#include "av_muxer.h"

bool AvMuxer::Open(std::string_view filePath, std::string_view format)
{
    Close();
    _isOpenFile = false;
    _filePath = filePath;
    __CheckBool(avformat_alloc_output_context2(&_fmtCtx, nullptr, format.data(), _filePath.c_str()) >= 0);
    __CheckBool(_fmtCtx);
    return true;
}

bool AvMuxer::WriteHeader()
{
    av_dump_format(_fmtCtx, 0, _filePath.data(), 1);
    // 打开输出文件
    if (!(_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        __CheckBool(avio_open(&_fmtCtx->pb, _filePath.c_str(), AVIO_FLAG_WRITE) >= 0);
    }
    // 写入文件头
    __CheckBool(avformat_write_header(_fmtCtx, nullptr) >= 0);
    _isOpenFile = true;
    return true;
}

int AvMuxer::AddVideoStream(const Encoder<MediaType::VIDEO>::Param& param)
{
    __Check(-1, _fmtCtx->oformat->video_codec != AV_CODEC_ID_NONE);
    Info info;
    info.pts = 0;
    info.fps = param.fps;
    auto encoder = new Encoder<MediaType::VIDEO>;
    __Check(-1, encoder->Open(param, _fmtCtx));
    info.type = MediaType::VIDEO;
    info.encoder = encoder;
    __Check(-1, _AddStream(info));
    _infos.back().stream->time_base = {1, info.fps};
    return info.streamIndex;
}

int AvMuxer::AddAudioStream(const Encoder<MediaType::AUDIO>::Param& param)
{
    __Check(-1, _fmtCtx->oformat->audio_codec != AV_CODEC_ID_NONE);
    Info info;
    info.pts = 0;
    info.fps = AUDIO_SAMPLE_RATE;
    auto encoder = new Encoder<MediaType::AUDIO>;
    info.type = MediaType::AUDIO;
    info.encoder = encoder;
    __Check(-1, encoder->Open(param, _fmtCtx));
    __Check(-1, _AddStream(info));
    _infos.back().stream->time_base = {1, AUDIO_SAMPLE_RATE};
    return info.streamIndex;
}

bool AvMuxer::Write(AVFrame* frame, int streamIndex, bool isEnd)
{
    // 此函数不能被多个流同时调用
    std::lock_guard<std::mutex> lk(_mtx);
    __CheckBool(_infos.size() > streamIndex);
    auto&& info = _infos[streamIndex];
    if (info.isEnd) {
        return true;
    }
    if (isEnd) {
        info.isEnd = isEnd;
        frame = nullptr;
    }
    __CheckBool(info.encoder);
    // 检测流之间时间是不是差的太多，如果差的太多，直接弃掉数据多的流数据
    if (!_CheckTime(double(info.pts) / info.fps)) {
        info.isEncodeOverload = true;
        return false;
    }
    info.isEncodeOverload = false;
    __CheckBool(info.encoder->PushFrame(frame, isEnd, info.pts));
    info.pts += info.type == MediaType::AUDIO ? info.encoder->GetCtx()->frame_size : 1; // 更新 pts
    AVPacket* packet = nullptr;
    while ((packet = info.encoder->Encode())) {
        av_packet_rescale_ts(packet, info.encoder->GetCtx()->time_base, info.stream->time_base);
        packet->stream_index = info.stream->index;
        __DebugPrint("pts : %lld frame_size: %d", info.pts, info.encoder->GetCtx()->frame_size);
        __CheckBool(av_interleaved_write_frame(_fmtCtx, packet) >= 0);
        // auto ret = av_interleaved_write_frame(_fmtCtx, packet);
        //__DebugPrint("%d", ret);
        //  if (ret < 0) {
        //     break;
        // }
    }
    info.encoder->AfterEncode();
    return true;
}

bool AvMuxer::_CheckTime(double time)
{
    auto minTime = double(_infos.front().pts) / _infos.front().fps;
    for (int idx = 1; idx < _infos.size(); ++idx) {
        minTime = std::min(double(_infos[idx].pts) / _infos[idx].fps, minTime);
    }
    if (time - minTime > 0.1) { // 说明相差的太多了，下一帧不能再送往编码器
        return false;
    }
    return true;
}

void AvMuxer::Close()
{
    if (_fmtCtx == nullptr) {
        return;
    }
    // 清空编码器缓存
    for (int index = 0; index < _infos.size(); ++index) {
        __DebugPrint("stream: %d, time:%f", index, double(_infos[index].pts) / _infos[index].fps);
    }
    if (_isOpenFile) {
        __CheckNo(av_write_trailer(_fmtCtx) >= 0);
        Free(_fmtCtx->pb, [this] { avio_closep(&_fmtCtx->pb); });
    }
    _isOpenFile = false;

    for (auto&& info : _infos) {
        info.encoder->Close();
        Free(info.encoder, [&info] {info.encoder->Close(); delete info.encoder; });
    }
    _infos.clear();
    Free(_fmtCtx, [this] { avformat_free_context(_fmtCtx); });
}

bool AvMuxer::_AddStream(Info& info)
{
    __CheckBool(info.stream = avformat_new_stream(_fmtCtx, nullptr));
    info.stream->id = _fmtCtx->nb_streams - 1;
    __CheckBool(avcodec_parameters_from_context(info.stream->codecpar, info.encoder->GetCtx()) >= 0);
    info.streamIndex = _fmtCtx->nb_streams - 1;
    info.pts = 0;
    info.isEnd = false;
    _infos.push_back(info);
    return true;
}

AVCodecContext* AvMuxer::GetCodecCtx(int streamIndex)
{
    __CheckNullptr(streamIndex >= 0 && _infos.size() > streamIndex);
    return _infos[streamIndex].encoder->GetCtx();
}

bool AvMuxer::IsEncodeOverload() const
{
    for (auto&& info : _infos) {
        if (info.isEncodeOverload) {
            return true;
        }
    }
    return false;
}