/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-01-31 17:19:09
 * @Description:
 */
#ifndef __MUXER_H__
#define __MUXER_H__

#include "av_encoder.h"

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

class AvMuxer {
public:
    ~AvMuxer()
    {
        Close();
    }
    bool Open(std::string_view filePath, int width, int height, int fps = 30)
    {
        Close();
        _fps = fps;
        _totalTick = 0;
        avformat_alloc_output_context2(&_fmtCtx, nullptr, nullptr, filePath.data());
        if (_fmtCtx == nullptr) {
            __DebugPrint("avformat_alloc_output_context2 failed\n");
            return false;
        }

        if (_fmtCtx->oformat->video_codec != AV_CODEC_ID_NONE) {
            if (!_OpenEncoderAndAddStream(width, height)) {
                __DebugPrint("_OpenEncoderAndAddStream failed\n");
                return false;
            }
        }
        av_dump_format(_fmtCtx, 0, filePath.data(), 1);

        // 打开输出文件
        if (!(_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&_fmtCtx->pb, filePath.data(), AVIO_FLAG_WRITE) < 0) {
                __DebugPrint("avio_open failed\n");
                return false;
            }
        }

        // 写入文件头
        if (avformat_write_header(_fmtCtx, nullptr) < 0) {
            __DebugPrint("avformat_write_header failed\n");
            return false;
        }
        return true;
    }

    bool Write(AVFrame* frame, bool isEnd = false)
    {
        if (_encoder == nullptr) {
            return false;
        }
        auto&& info = _encoder->Encode(frame, isEnd);
        _totalTick += info.size;
        for (int idx = 0; idx < info.size; ++idx) {
            auto&& pkt = info.data[idx].packet;
            av_packet_rescale_ts(pkt, _codecCtx->time_base, _stream->time_base);
            pkt->stream_index = _stream->index;
            if (av_interleaved_write_frame(_fmtCtx, pkt) < 0) {
                __DebugPrint("av_interleaved_write_frame failed\n");
                return false;
            }
        }
        return true;
    }

    void Close()
    {
        if (_fmtCtx == nullptr) {
            return;
        }
        Write(nullptr, true); // 清空编码器缓存
        av_write_trailer(_fmtCtx);
        _encoder->Close();
        Free(_fmtCtx->pb, [this] { avio_closep(&_fmtCtx->pb); });
        Free(_encoder, [this] { delete _encoder; });
        Free(_fmtCtx, [this] { avformat_free_context(_fmtCtx); });
        __DebugPrint("_totalTick: %d\n", _totalTick);
    }

private:
    AVFormatContext* _fmtCtx = nullptr;
    AVStream* _stream = nullptr;
    AVCodecContext* _codecCtx = nullptr;
    int _fps = 30;
    AvEncoder* _encoder;
    int _totalTick = 0;

    bool _AddStream()
    {
        _stream = avformat_new_stream(_fmtCtx, nullptr);
        if (_stream == nullptr) {
            __DebugPrint("avformat_new_stream failed\n");
            return false;
        }
        _stream->time_base = {1, _fps};
        _stream->id = _fmtCtx->nb_streams - 1;
        return true;
    }

    bool _OpenEncoderAndAddStream(int width, int height)
    {
        _encoder = new AvEncoder;
        EncodeParam param;
        param.bitRate = 4000'000;
        param.fmtCtx = _fmtCtx;
        param.fps = _fps;
        param.width = width;
        param.height = height;
        param.name = "h264_nvenc";
        // param.name = "libx264";
        if (!_encoder->Open(param)) {
            __DebugPrint("encoder->Open failed\n");
            return false;
        }

        if (!_AddStream()) {
            __DebugPrint("_AddStream failed\n");
            return false;
        }
        _codecCtx = _encoder->GetCtx();
        if (avcodec_parameters_from_context(_stream->codecpar, _codecCtx) > 0) {
            __DebugPrint("avcodec_parameters_from_context failed\n");
            return false;
        }
        return true;
    }
};

#endif