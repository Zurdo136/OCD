/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    device.h

Abstract:

    Declarations of WDF device specific entry points

Environment:

    kernel mode only

Revision History:

--*/

#pragma once

#include "trace.h"
#include <wdm.h>
#include <wdf.h>

//
// The total time window for an input pattern. 60 seconds
//
#define INPUT_PATTERN_MAX_PERIOD    600000000

//
// Number of swipes for an input pattern
//
#define INPUT_PATTERN_NUM_OF_SWIPES 3

//
// The max time for a single swipe. 15 seconds
//
#define SINGLE_SWIPE_MAX_PERIOD     150000000

//
// The min time for a single swipe. 5 seconds
//
#define SINGLE_SWIPE_MIN_PERIOD     50000000

//
// the max interval between 2 touch event in a single swipe. 0.2 seconds
//
#define SINGLE_SWIPE_MAX_INTERVAL   2000000


//
// macros as defined in the WP Input Drivers for Monitor Power State
//
#define MONITOR_IS_OFF 0  
#define MONITOR_IS_ON  1

//
// Device context
//

typedef struct _DEVICE_EXTENSION
{
    //
    // Interrupt servicing
    //
    WDFINTERRUPT InterruptObject;

    //
    // Count of swipes
    //
    ULONG NumberOfSwipes;

    //
    // Time stamp of the first touch input of the input pattern.
    //
    ULONGLONG TimeStampInputPatternBegin;

    //
    // Time stamp of the first touch input of current swipe.
    //
    ULONGLONG TimeStampSwipeBegin;

    //
    // Time stamp of the last interrupt.
    //
    ULONGLONG TimeStampLastInterrupt;

    //
    // Inidcates that user has swipped for enough time.
    //
    BOOLEAN SwipeThreholdReached;
	
	PVOID MonitorChangeNotificationHandle;
	ULONG LastProcessedMonitorState;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION, GetDeviceContext)

EVT_WDF_INTERRUPT_ISR WpCrasherEvtInterruptIsr;

POWER_SETTING_CALLBACK WpCrasherOnPowerSettingsChange; 

EVT_WDF_OBJECT_CONTEXT_CLEANUP WpCrasherEvtDeviceContextCleanup; 


