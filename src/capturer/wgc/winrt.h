#pragma once

// WinRT
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Popups.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>

#include <windows.ui.composition.interop.h>
#include <DispatcherQueue.h>

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Composition::Desktop;

// Direct3D11CaptureFramePool requires a DispatcherQueue
winrt::Windows::System::DispatcherQueueController CreateDispatcherQueueController();
DesktopWindowTarget CreateDesktopWindowTarget(Compositor const& compositor, HWND window);
