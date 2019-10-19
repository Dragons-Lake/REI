/*
 * This file is based on The-Forge source code
 * (see https://github.com/ConfettiFX/The-Forge).
*/

#pragma once

#include "../Interface/Common.h"

#if defined(_WIN32)
#    include <sys/stat.h>
#    include <stdlib.h>
#    ifndef NOMINMAX
#        define NOMINMAX 1
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN 1
#    endif
#    include <windows.h>
#endif

#if _MSC_VER >= 1400
#pragma warning(disable : 4996)
#endif

#if defined(__APPLE__)
#    if !defined(TARGET_IOS)
#        import <Carbon/Carbon.h>
#    else
#        include <stdint.h>
typedef uint64_t uint64;
#    endif
#endif
#if defined(__ANDROID__)
#    include <android/log.h>
#elif defined(__linux__)
#    define VK_USE_PLATFORM_XLIB_KHR
#    if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
#        include <X11/Xutil.h>
#    endif
#endif

typedef struct REI_WindowHandle
{
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    Display* display;
    Window   window;
    Atom     xlib_wm_delete_window;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    xcb_connection_t*        connection;
    xcb_window_t             window;
    xcb_screen_t*            screen;
    xcb_intern_atom_reply_t* atom_wm_delete_window;
#else
    void* window;    //hWnd
#endif
} REI_WindowHandle;

