/*++
    Copyright (c) Microsoft Corporation

    Module Name:

        driver.h

    Abstract:

        Contains WDF driver-specific function declarations

    Environment:

        Kernel mode

    Revision History:

--*/

#pragma once

#include "trace.h"
#include <wdm.h>
#include <wdf.h>


DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DEVICE_CONTEXT_CLEANUP OnContextCleanup;

EVT_WDF_DRIVER_DEVICE_ADD OnDeviceAdd;

