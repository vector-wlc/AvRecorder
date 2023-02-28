<!--
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-12 13:05:50
 * @Description: 
-->
# AvRecorder
Record audio and video via WGC, DXGI, BitBlt, CoreAudio, DirectX11 and FFmpeg

Built by MinGW_64 11.2.0 + Qt_64 6.4.2 + FFmpeg 5.1.0

## 目前完成的功能

* 使用 DXGI 录制桌面
* 使用 BitBlt 录制窗口
* 使用 WGC 捕获桌面和窗口
* 多显示器的支持
* 使用 DirectX11 对捕获的画面进行渲染
* 使用 FFmpeg 对捕获的画面进行编码
* 使用 CoreAudio 录制麦克风和扬声器
* 使用 FFmpeg 对麦克风和扬声器进行混音，并且具备控制音频音量的功能
* 使用 FFmpeg 对音频和视频进行编码
* 可以对音频进行调幅
* 帧率控制

## 待完成功能(咕咕咕)

* 暂停录制
* 截取画面
* 画面合成
* 推流功能

## 遇到的坑

* 鼠标绘制方面：不要使用 DrawIcon, 而应该使用 DrawIconEx 并且最后的参数设置为 DI_NORMAL | DI_COMPAT, 不然绘制出来的鼠标会有锯齿
* DXGI 截屏为了能够让 GDI 进行鼠标绘制, _textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE 应这样设置
* 帧率控制: Sleep 千万不要简单的让时间除以帧率, 因为 C++ 中整数运算会直接截断, 导致视频的截取的频率会变快
* GDI 截屏将画面复制到 AVFrame 时要一行一行的复制, 不然画面可能会撕裂，原因是内存对齐
* 硬件编码必须每次 alloc_frame 和 free_frame, 不然会导致内存泄漏
* 音频混音过滤链必须每次 unref, 不然会导致内存泄漏
* 音频捕获当电脑没有播放器播放声音时, 扬声器会停止工作, 这会导致一系列严重的问题: 混音失败、编码失败、音画不同步等, 这里采用的方案是循环播放一个静音的音频，强制让扬声器工作
* DXGI 截屏当桌面没有画面刷新时, 会返回一个错误码, 不过这没事, 直接让编码器编码上一帧的缓存即可

## 关于性能
* CPU 软件编码设置为 veryfast, 以降低 CPU 的占用
* 截取画面从 GPU 到 CPU 需要调用 Map 函数, 这个函数实际上很坑的, 因为他必须等待 GPU 把这帧画面绘制完成才能工作, 这样 CPU 就得搁那干等, 究极浪费性能, 解决方案就是多缓存几个 Texture, 这样做就是增加了延迟, 但是这无所谓, 录屏谁在乎那几帧的延迟呢,肉眼是看不出来的
* 尚未解决：像素格式转换 CPU 占用高, 据说 D3D11 能在硬件层面完成像素转换