<!--
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-12 13:05:50
 * @Description: 
-->
# AvRecorder
Record audio and video via DXGI, BitBlt, CoreAudio, DirectX11 and FFmpeg

Built by MinGW_64 11.2.0 + Qt_64 6.4.2

## 目前完成的功能

* 使用 DXGI 录制桌面
* 使用 BitBlt 录制窗口
* 使用 DirectX11 对捕获的画面进行渲染
* 使用 FFmpeg 对捕获的画面进行编码
* 帧率控制

## 待完成功能

* 使用 CoreAudio 录制麦克风和扬声器
* 使用 FFmpeg 对麦克风和扬声器进行混音，并且具备控制音频音量的功能
* 使用 FFmpeg 对音频和视频进行编码
* 优化界面编写
* 推流功能(这个可能会鸽)