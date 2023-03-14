/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-24 16:28:40
 * @Description:
 */
#pragma once
#define UNICODE

#include <dwmapi.h>
#include <string>
#include <array>
#include <vector>
#include <d3d11.h>

class WindowFinder {
public:
    struct Info {
        HWND hwnd = nullptr;
        std::wstring title;
    };

    static const std::vector<Info>& GetList(bool isUpdate = false);

private:
    static std::vector<Info> _list;
    static std::wstring _GetWindowTextStd(HWND hwnd);
    static BOOL CALLBACK _EnumWindowsProc(HWND hwnd, LPARAM lParam);
    static bool IsAltTabWindow(HWND hwnd, const std::wstring& title);
};

class MonitorFinder {
public:
    struct Info {
        HMONITOR monitor = nullptr;
        std::wstring title;
        RECT rect;
    };

    static const std::vector<Info>& GetList(bool isUpdate = false);

private:
    static std::vector<Info> _list;

    static BOOL CALLBACK _MonitorEnumProc(
        HMONITOR hMonitor,  // handle to display monitor
        HDC hdcMonitor,     // handle to monitor-appropriate device context
        LPRECT lprcMonitor, // pointer to monitor intersection rectangle
        LPARAM dwData       // data passed from EnumDisplayMonitors
    );
};
