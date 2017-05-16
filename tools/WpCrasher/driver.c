/*++
    Copyright (c) Microsoft Corporation

    Module Name:

        driver.c

    Abstract:

        Contains driver-specific WDF functions

    Environment:

        Kernel mode

    Revision History:

--*/

#include <initguid.h>   
#include "driver.h"
#include "device.h"

#ifdef  WPP_TRACING
#include "driver.tmh"
#endif

#ifdef ALLOC_PRAGMA
  #pragma alloc_text(INIT, DriverEntry)
  #pragma alloc_text(PAGE, OnDeviceAdd)
  #pragma alloc_text(PAGE, OnContextCleanup)
#endif

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - pointer to the driver object
    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

    STATUS_SUCCESS if successful, error code otherwise.

--*/
{
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    //
    // Initialize tracing via WPP
    //
    WPP_INIT_TRACING( DriverObject, RegistryPath );

    //
    // Create a framework driver object
    //
    WDF_DRIVER_CONFIG_INIT(&config, OnDeviceAdd);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = OnContextCleanup;

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,
        &config,
        WDF_NO_HANDLE
        );

    if (!NT_SUCCESS(status))
    {
        TraceMessage(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_INIT,
            ("Error creating WDF driver object - %!STATUS!",
            status));

        WPP_CLEANUP(DriverObject);

        goto exit;
    }

exit:

    return status;
}

NTSTATUS
OnDeviceAdd(
    IN WDFDRIVER Driver,
    IN PWDFDEVICE_INIT DeviceInit
    )
/*++

Routine Description:

    OnDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager when a device is found.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry
    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS indicating success or failure

--*/
{
    WDF_OBJECT_ATTRIBUTES attributes;
    PDEVICE_EXTENSION devContext;
    WDFDEVICE fxDevice;
    WDF_INTERRUPT_CONFIG interruptConfig;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();

    //
    // Create a framework device object. This call will in turn create
    // a WDM device object, attach to the lower stack, and set the
    // appropriate flags and attributes.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_EXTENSION);

	//
	// Set the context cleanup routine to cleanup the device extension
	//
	attributes.EvtCleanupCallback = WpCrasherEvtDeviceContextCleanup;

    status = WdfDeviceCreate(&DeviceInit, &attributes, &fxDevice);

    if (!NT_SUCCESS(status))
    {
        TraceMessage(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_INIT,
            ("WdfDeviceCreate failed - %!STATUS!",
            status));

        goto exit;
    }

    devContext = GetDeviceContext(fxDevice);

    //
    // Create an interrupt object for hardware notifications
    //
    WDF_INTERRUPT_CONFIG_INIT(
        &interruptConfig,
        WpCrasherEvtInterruptIsr,
        NULL);

    interruptConfig.PassiveHandling = FALSE;

    status = WdfInterruptCreate(
        fxDevice,
        &interruptConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &devContext->InterruptObject);

    if (!NT_SUCCESS(status))
    {
        TraceMessage(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_INIT,
            ("Error creating WDF interrupt object - %!STATUS!",
            status));

        goto exit;
    }

    devContext->NumberOfSwipes = 0;
    devContext->TimeStampInputPatternBegin = 0;
    devContext->TimeStampSwipeBegin = 0;
    devContext->TimeStampLastInterrupt = 0;
    devContext->SwipeThreholdReached = FALSE;
	
	devContext->LastProcessedMonitorState = MONITOR_IS_ON;

	//
    //  Register a callback which gets called when the monitor turns on/off
	//  This Callback soft-connects/disconnects the interrupt based on the monitor state
	// 
	status = PoRegisterPowerSettingCallback(  
            WdfDeviceWdmGetDeviceObject(fxDevice),  
            &GUID_MONITOR_POWER_ON,  
            WpCrasherOnPowerSettingsChange,  
            (PVOID)fxDevice,  
            &devContext->MonitorChangeNotificationHandle); 

    if (!NT_SUCCESS(status))  
    {  
        TraceMessage(  
            TRACE_LEVEL_ERROR,  
            TRACE_FLAG_INIT,  
            ("Could not register for monitor state changes - %!STATUS!",  
            status));  
  
        goto exit;  
    }


exit:

    return status;
}

VOID
OnContextCleanup(
    IN WDFOBJECT Driver
    )
/*++
Routine Description:

    Free resources allocated in DriverEntry that are not automatically
    cleaned up framework.

Arguments:

    Driver - handle to a WDF Driver object.

Return Value:

    VOID.

--*/
{
    PAGED_CODE();

#pragma prefast(suppress: 26010, "Warning is not due to this driver code")
    WPP_CLEANUP(WdfDriverWdmGetDriverObject(Driver));
}
