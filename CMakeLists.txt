cmake_minimum_required(VERSION 3.5) # CMake install : https://cmake.org/download/
project(AvRecorder LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_PREFIX_PATH "e:/Qt/6.4.2/mingw_64") # Qt Kit Dir
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})

# ffmpeg
include_directories(e:/ffmpeg/include)
link_directories(e:/ffmpeg/lib)

include_directories(./src)
include_directories(e:/Qt/Tools/mingw1120_64/lib/gcc/x86_64-w64-mingw32/11.2.0/include/c++)
include_directories(e:/Qt/Tools/mingw1120_64/lib/gcc/x86_64-w64-mingw32/11.2.0/include/c++/x86_64-w64-mingw32)

find_package(Qt6 COMPONENTS Widgets REQUIRED) # Qt COMPONENTS
aux_source_directory(./src/basic basicSrcs)
aux_source_directory(./src/capturer capturerSrcs)
aux_source_directory(./src/encoder encoderSrcs)
aux_source_directory(./src/muxer muxerSrcs)
aux_source_directory(./src/recorder recorderSrcs)
aux_source_directory(./src/ui uiSrcs)

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
    src/main.cpp
    rc/icon.rc
) 
target_link_libraries(
    ${PROJECT_NAME} 
    # Qt6 Shared Library
    PRIVATE Qt6::Widgets 

    # ffmpeg
    avcodec.dll
    avdevice.dll 
    avfilter.dll 
    avformat.dll
    avutil.dll
    swresample.dll
    postproc.dll
    swscale.dll

    # windows
    ksuser
    d3d11
    user32
    gdiplus
    winmm
)