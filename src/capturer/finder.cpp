#include "finder.h"

const std::vector<WindowFinder::Info>& WindowFinder::GetList(bool isUpdate)
{
    if (!isUpdate) {
        return _list;
    }
    _list.clear();
    EnumWindows(_EnumWindowsProc, (LPARAM) nullptr);
    return _list;
}

std::vector<WindowFinder::Info> WindowFinder::_list;

std::wstring WindowFinder::_GetWindowTextStd(HWND hwnd)
{
    std::array<WCHAR, 1024> windowText;
    ::GetWindowTextW(hwnd, windowText.data(), (int)windowText.size());
    std::wstring title(windowText.data());
    return title;
}
BOOL CALLBACK WindowFinder::_EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto title = _GetWindowTextStd(hwnd);
    if (!IsAltTabWindow(hwnd, title)) {
        return TRUE;
    }
    _list.push_back({hwnd, std::move(title)});
    return TRUE;
}

bool WindowFinder::IsAltTabWindow(HWND hwnd, const std::wstring& title)
{
    HWND shellWindow = GetShellWindow();

    if (hwnd == shellWindow) {
        return false;
    }

    if (title.length() == 0 || title == L"NVIDIA GeForce Overlay") {
        return false;
    }

    if (!IsWindowVisible(hwnd)) {
        return false;
    }

    if (GetAncestor(hwnd, GA_ROOT) != hwnd) {
        return false;
    }

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if (!((style & WS_DISABLED) != WS_DISABLED)) {
        return false;
    }

    DWORD cloaked = FALSE;
    HRESULT hrTemp = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (SUCCEEDED(hrTemp) && cloaked == DWM_CLOAKED_SHELL) {
        return false;
    }

    return !IsIconic(hwnd);
}

const std::vector<MonitorFinder::Info>& MonitorFinder::GetList(bool isUpdate)
{
    if (!isUpdate) {
        return _list;
    }
    _list.clear();
    EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProc, (LPARAM) nullptr);
    _DxgiMonitorEnum();
    return _list;
}

std::vector<MonitorFinder::Info> MonitorFinder::_list;

BOOL CALLBACK MonitorFinder::_MonitorEnumProc(
    HMONITOR hMonitor,  // handle to display monitor
    HDC hdcMonitor,     // handle to monitor-appropriate device context
    LPRECT lprcMonitor, // pointer to monitor intersection rectangle
    LPARAM dwData       // data passed from EnumDisplayMonitors
)
{
    std::wstring name = L"显示器" + std::to_wstring(_list.size() + 1);
    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(hMonitor, &monitorInfo);
    Info info;
    info.monitor = hMonitor;
    info.rect = monitorInfo.rcMonitor;
    info.dxgi = nullptr;
    info.title = std::move(name);
    _list.push_back(std::move(info));
    return TRUE;
}

void MonitorFinder::_DxgiMonitorEnum()
{
    IDXGIAdapter* pAdapter;
    IDXGIFactory* pFactory = NULL;
    // Create a DXGIFactory object.
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) {
        return;
    }
    for (UINT i = 0;
         pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND;
         ++i) {
        if (i < _list.size()) {
            _list[i].dxgi = pAdapter;
        }
    }

    if (pFactory) {
        pFactory->Release();
    }
}
