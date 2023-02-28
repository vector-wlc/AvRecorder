/*
 * @Coding: utf-8
 * @Author: vector-wlc
 * @Date: 2023-02-24 16:28:40
 * @Description:
 */

#include "pch.h"
#include "App.h"
#include "SimpleCapture.h"
#include <ShObjIdl.h>

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Composition::Desktop;

// Direct3D11CaptureFramePool requires a DispatcherQueue
winrt::Windows::System::DispatcherQueueController CreateDispatcherQueueController()
{
    namespace abi = ABI::Windows::System;

    DispatcherQueueOptions options {
        sizeof(DispatcherQueueOptions),
        DQTYPE_THREAD_CURRENT,
        DQTAT_COM_STA};

    Windows::System::DispatcherQueueController controller {nullptr};
    check_hresult(CreateDispatcherQueueController(options, reinterpret_cast<abi::IDispatcherQueueController**>(put_abi(controller))));
    return controller;
}

DesktopWindowTarget CreateDesktopWindowTarget(Compositor const& compositor, HWND window)
{
    namespace abi = ABI::Windows::UI::Composition::Desktop;
    auto interop = compositor.as<abi::ICompositorDesktopInterop>();
    DesktopWindowTarget target {nullptr};
    check_hresult(interop->CreateDesktopWindowTarget(window, true, reinterpret_cast<abi::IDesktopWindowTarget**>(put_abi(target))));
    return target;
}
