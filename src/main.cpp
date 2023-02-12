/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-03 10:00:08
 * @Description:
 */

#include "audio_capturer.h"
#include "av_recorder.h"
#include <QApplication>

struct Test {
    int a;
};

int main(int argc, char* argv[])
{
    std::is_trivial<Test>::type c;
    QApplication a(argc, argv);
    AvRecorder w;
    w.show();
    return a.exec();
}
