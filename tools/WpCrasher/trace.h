/*++
    Copyright (c) Microsoft Corporation

    Module Name:

        trace.h

    Abstract:

        Contains prototypes and definitions related to debugging/tracing

    Environment:

        Kernel mode

    Revision History:

--*/

#pragma once

//
// Control GUID:
// {37a4ecfe-43e8-4b83-ac14-533fb83d8d4f}
//
#ifdef  WPP_TRACING

#define WPP_CONTROL_GUIDS                        \
    WPP_DEFINE_CONTROL_GUID(                     \
        WpCrasherControlGuid,                    \
        (37a4ecfe,43e8,4b83,ac14,533fb83d8d4f),  \
        WPP_DEFINE_BIT(TRACE_FLAG_INIT)          \
        WPP_DEFINE_BIT(TRACE_FLAG_REPORTING)     \
        WPP_DEFINE_BIT(TRACE_FLAG_INTERRUPT)     \
        WPP_DEFINE_BIT(TRACE_FLAG_OTHER)         \
        )

#define WPP_LEVEL_FLAGS_LOGGER(level,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(level,flags) \
    (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= level)
#else

#define WPP_INIT_TRACING(...)
#define WPP_CLEANUP(x) x

#define TraceMessage(_level_,_flag_,_msg_, ...)  

#endif

