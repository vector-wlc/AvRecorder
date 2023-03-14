cmake_minimum_required(VERSION 3.5) # CMake install : https://cmake.org/download/
project(AvRecorder)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_PREFIX_PATH "d:/Software/Qt/5.15.0/msvc2019_64") # Qt Kit Dir
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ffmpeg
include_directories(d:/ffmpeg/include)
link_directories(d:/ffmpeg/lib)

include_directories(./src)


find_package(Qt5 COMPONENTS Widgets REQUIRED) # Qt COMPONENTS
aux_source_directory(./src/basic basicSrcs)
aux_source_directory(./src/capturer capturerSrcs)
aux_source_directory(./src/encoder encoderSrcs)
aux_source_directory(./src/muxer muxerSrcs)
aux_source_directory(./src/recorder recorderSrcs)
aux_source_directory(./src/ui uiSrcs)
aux_source_directory(./src/capturer/wgc wgcSrcs)
aux_source_directory(./src/d3d d3dSrcs)

# Specify MSVC UTF-8 encoding   
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_executable(${PROJECT_NAME}
    # WIN32 # If you need a terminal for debug, please comment this statement 
    ${basicSrcs} 
    ${capturerSrcs} 
    ${encoderSrcs} 
    ${muxerSrcs} 
    ${recorderSrcs} 
    ${uiSrcs} 
    ${wgcSrcs} 
    ${d3dSrcs} 
    src/main.cpp
    rc/icon.rc
) 

target_link_libraries(
    ${PROJECT_NAME} 
    # Qt6 Shared Library
    PRIVATE Qt5::Widgets 

    # ffmpeg
    avcodec
    avdevice 
    avfilter 
    avformat
    avutil
    swresample
    postproc
    swscale

    # windows
    ksuser
    d3d11
    user32
    winmm
    windowsapp
    dwmapi
    dxgi
)