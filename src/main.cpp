/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-03 10:00:08
 * @Description:
 */
#include "ui/av_recorder.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    AvRecorder w;

    w.show();
    return a.exec();
}
