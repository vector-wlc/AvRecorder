/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-01-31 17:19:09
 * @Description:
 */
#ifndef __AV_MUXER_H__
#define __AV_MUXER_H__

#include "encoder/audio_encoder.h"
#include "encoder/video_encoder.h"

class AvMuxer {
public:
    struct Info {
        MediaType type;
        AbstractEncoder* encoder = nullptr;
        AVStream* stream = nullptr;
        int streamIndex = -1;
        int fps = 30;
        uint64_t pts = 0;
        bool isEnd = false;
        bool isEncodeOverload = false;
    };
    ~AvMuxer()
    {
        Close();
    }
    bool Open(std::string_view filePath, std::string_view format = "mp4");
    bool WriteHeader();
    // 返回值为创建的流的索引 ，-1表示创建失败
    int AddVideoStream(const Encoder<MediaType::VIDEO>::Param& param);
    int AddAudioStream(const Encoder<MediaType::AUDIO>::Param& param);
    bool Write(AVFrame* frame, int streamIndex, bool isEnd = false);
    void Close();
    AVCodecContext* GetCodecCtx(int streamIndex);
    bool IsEncodeOverload() const;

private:
    std::mutex _mtx;
    bool _isOpenFile = false;
    bool _AddStream(Info& info);
    bool _CheckTime(double time);
    std::vector<Info> _infos;
    AVFormatContext* _fmtCtx = nullptr;
    std::string _filePath;
};

#endif