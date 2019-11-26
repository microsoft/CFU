/*++

    Copyright (c) Microsoft Corporation. All rights reserved
    Licensed under the MIT license.

Module Name:

    DmfInterface.c

Abstract:

    Instantiate Dmf Library Modules used by this driver.
    
Environment:

    User-mode Driver Framework

--*/
#include "Common.h"

#include "DmfInterface.tmh"

///////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE
///////////////////////////////////////////////////////////////////////////////////////////

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD ComponentFirmwareUpdateEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP ComponentFirmwareUpdateEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD ComponentFirmwareUpdateDeviceModulesAdd;

/* WPP_INIT_TRACING(); (Comment required by WPP parser)*/
VOID
ComponentFirmwareUpdateEvtDriverContextCleanup(
    WDFOBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);
    WPP_CLEANUP(DriverObject);
}

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES attributes;

    WPP_INIT_TRACING(DriverObject, 
                     RegistryPath);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = ComponentFirmwareUpdateEvtDriverContextCleanup;
    WDF_DRIVER_CONFIG_INIT(&config,
                           ComponentFirmwareUpdateEvtDeviceAdd);

    ntStatus = WdfDriverCreate(DriverObject,
                               RegistryPath,
                               &attributes,
                               &config,
                               WDF_NO_HANDLE);
    if (! NT_SUCCESS(ntStatus))
    {
        WPP_CLEANUP(DriverObject);
        goto Exit;
    }

Exit:

    return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS
ComponentFirmwareUpdateEvtDeviceAdd(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit
    )
/*++

Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. Create and initialize a device object to
    represent a new instance of the device.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry
    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;

    WDF_OBJECT_ATTRIBUTES attributes;
    PDMFDEVICE_INIT dmfDeviceInit;
    DMF_EVENT_CALLBACKS dmfCallbacks;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    FuncEntry(TRACE_DRIVER);

    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);

    // Tell WDF that this driver do not need PnP Power callbacks.
    //
    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit,
                                                NULL);
    // All DMF drivers must call this function even if they do not support File Object callbacks.
    //
    DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit,
                                          NULL);
    // All DMF drivers must call this function even if they do not support Power Policy callbacks.
    //
    DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit,
                                                   NULL);

    // Tell the framework that we are a filter driver. Framework
    // takes care of inheriting all the device flags & characterstics
    // from the lower device we are attaching to.
    //
    WdfFdoInitSetFilter(DeviceInit);
    // DMF Client drivers that are filter drivers must also make this call.
    //
    DMF_DmfFdoSetFilter(dmfDeviceInit);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, 
                                            DEVICE_CONTEXT);
    ntStatus = WdfDeviceCreate(&DeviceInit,
                               &attributes,
                               &device);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DRIVER, 
                    "WdfDeviceCreate fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    deviceContext = DeviceContextGet(device);

    // Create a collection to hold the firmware information.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device;
    ntStatus = WdfCollectionCreate(&attributes,
                                   &deviceContext->FirmwareBlobCollection);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, 
                    TRACE_DEVICE, 
                    "WdfCollectionCreate fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    DMF_EVENT_CALLBACKS_INIT(&dmfCallbacks);
    dmfCallbacks.EvtDmfDeviceModulesAdd = ComponentFirmwareUpdateDeviceModulesAdd;
    DMF_DmfDeviceInitSetEventCallbacks(dmfDeviceInit,
                                       &dmfCallbacks);

    ntStatus = DMF_ModulesCreate(device,
                                 &dmfDeviceInit);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DRIVER, 
                    "DMF_ModulesCreate fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }
    
Exit:

    if (dmfDeviceInit != NULL)
    {
        DMF_DmfDeviceInitFree(&dmfDeviceInit);
    }

    FuncExit(TRACE_DRIVER, "ntStatus=%!STATUS!", ntStatus);
    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
CFUHidTransport_PostOpen_Callback(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This is callback to indicate that CFU Hid Transport module is opened (Post Open)

Arguments:

    DmfModule - The Module from which the callback is called.

Return Value:

    None.

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;

    PAGED_CODE();

    FuncEntry(TRACE_DEVICE);

    device = DMF_ParentDeviceGet(DmfModule);
    deviceContext = DeviceContextGet(device);

    // Bind the Modules using SampleInterface Interface.
    // The decision about which Transport to bind has already been made and the
    // Transport Module has already been created.
    //
    ntStatus = DMF_INTERFACE_BIND(deviceContext->DmfModuleComponentFirmwareUpdate,
                                  deviceContext->DmfModuleComponentFirmwareUpdateTransportHid,
                                  ComponentFirmwareUpdate);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "DMF_INTERFACE_BIND fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "Issuing Protocol Start.");

    ntStatus = DMF_ComponentFirmwareUpdate_Start(deviceContext->DmfModuleComponentFirmwareUpdate);
    if (!NT_SUCCESS(ntStatus))
    {
        NTSTATUS ntStatus2;
        WDFMEMORY deviceHardwareIdentifierMemory;
        PWSTR deviceHardwareIdentifier;

        // Get the Device's HardwareID.
        //
        deviceHardwareIdentifierMemory = WDF_NO_HANDLE;
        deviceHardwareIdentifier = L"";
        ntStatus2 = WdfDeviceAllocAndQueryProperty(device,
                                                   DevicePropertyHardwareID, 
                                                   PagedPool, 
                                                   WDF_NO_OBJECT_ATTRIBUTES, 
                                                   &deviceHardwareIdentifierMemory);
        if (NT_SUCCESS(ntStatus2))
        {
            deviceHardwareIdentifier = (PWSTR) WdfMemoryGetBuffer(deviceHardwareIdentifierMemory,
                                                                  NULL);
        }

        PWCHAR formatStrings[] = { L"HardwareId=%s", L"ntStatus=0x%x" };

        // Report the failure in Event Log.
        //
        DMF_Utility_EventLogEntryWriteUserMode(EVENTLOG_PROVIDER_NAME,
                                               EVENTLOG_ERROR_TYPE,
                                               EVENTLOG_MESSAGE_PROTOCOL_START_FAIL,
                                               ARRAYSIZE(formatStrings),
                                               formatStrings,
                                               ARRAYSIZE(formatStrings),
                                               deviceHardwareIdentifier,
                                               ntStatus);

        // Failure in starting the protocol is non-recoverable.
        // Report the failure to framework and let it attempt to restart the driver.
        // Note: This may result in banged out devnode.
        //
        WdfDeviceSetFailed(device,
                           WdfDeviceFailedAttemptRestart);

        goto Exit;
    }

Exit:

    FuncExitVoid(TRACE_DEVICE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
CFUHidTransport_PreClose_Callback(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This is callback to indicate that CFU Hid Transport is about to be closed (Pre Close)

Arguments:

    DmfModule - The Module from which the callback is called.

Return Value:

    None.

--*/
{
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;

    FuncEntry(TRACE_DEVICE);

    PAGED_CODE();

    device = DMF_ParentDeviceGet(DmfModule);
    deviceContext = DeviceContextGet(device);

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "CFU Core Closed.");

    DMF_ComponentFirmwareUpdate_Stop(deviceContext->DmfModuleComponentFirmwareUpdate);

    DMF_INTERFACE_UNBIND(deviceContext->DmfModuleComponentFirmwareUpdate,
                         deviceContext->DmfModuleComponentFirmwareUpdateTransportHid,
                         ComponentFirmwareUpdate);

    FuncExitVoid(TRACE_DEVICE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
ComponentFirmwareUpdateDeviceModulesAdd(
    _In_ WDFDEVICE Device,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Instantiate all the Dmf Modules used by this driver.

Parameters:

      Device: WDFDEVICE handle.
      DmfModuleInit - Opaque object to pass into DMF_ModuleAdd.

Return:

    None.

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_MODULE_EVENT_CALLBACKS moduleCallbacks;
    DMF_CONFIG_ComponentFirmwareUpdate componentFirmwareUpdateConfig;
    DMF_CONFIG_ComponentFirmwareUpdateHidTransport hidTransportConfig;

    PDEVICE_CONTEXT deviceContext;
    ULONG numberOfFirmwareComponents;

    PAGED_CODE();

    FuncEntry(TRACE_DEVICE);

    deviceContext = DeviceContextGet(Device);

    // Get Firmware information from the registry.
    //
    ntStatus = Registry_DeviceRegistryEnumerateAllFirwareSubKeys(Device);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] DeviceRegistryEnumerateAllFirwareSubKeys fails: ntStatus=%!STATUS!", 
                    Device,
                    ntStatus);
        goto Exit;
    }
    numberOfFirmwareComponents = WdfCollectionGetCount(deviceContext->FirmwareBlobCollection);

    // It is possible that the driver may not have the firmware information yet.
    // This is a valid case when the extension package is not yet installed in the target.
    // Don't create the CFU modules in such cases.
    //
    if (0 == numberOfFirmwareComponents)
    {
        TraceEvents(TRACE_LEVEL_WARNING, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] No firmware information available yet!  Not creating the CFU components.", 
                    Device);
        goto Exit;
    }

    // Component Firmware Update
    // -------------------------
    //
    DMF_CONFIG_ComponentFirmwareUpdate_AND_ATTRIBUTES_INIT(&componentFirmwareUpdateConfig,
                                                           &moduleAttributes);

    componentFirmwareUpdateConfig.SupportResumeOnConnect = deviceContext->CfuProtocolConfiguration.SupportResumeOnConnect;
    componentFirmwareUpdateConfig.SupportProtocolTransactionSkipOptimization = deviceContext->CfuProtocolConfiguration.SupportProtocolTransactionSkipOptimization;
    componentFirmwareUpdateConfig.NumberOfFirmwareComponents = numberOfFirmwareComponents;
    componentFirmwareUpdateConfig.EvtComponentFirmwareUpdateFirmwareOfferGet = ComponentFirmwareUpdateOfferGet;
    componentFirmwareUpdateConfig.EvtComponentFirmwareUpdateFirmwarePayloadGet = ComponentFirmwareUpdatePayloadGet;

    moduleAttributes.ClientModuleInstanceName = "ComponentFirmwareUpdate";

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &deviceContext->DmfModuleComponentFirmwareUpdate);

    // ComponentFirmwareUpdateHidTransport
    // -----------------------------------
    //
    DMF_CONFIG_ComponentFirmwareUpdateHidTransport_AND_ATTRIBUTES_INIT(&hidTransportConfig,
                                                                       &moduleAttributes);

    hidTransportConfig.Protocol = deviceContext->CfuHidTransportConfiguration.Protocol;
    hidTransportConfig.NumberOfInputReportReadsPended = deviceContext->CfuHidTransportConfiguration.NumberOfInputReportReadsPended;
    // No alignment requirement.
    //
    hidTransportConfig.PayloadFillAlignment = 1;

    moduleAttributes.ClientModuleInstanceName = "ComponentFirmwareUpdateHidTransport";

    DMF_MODULE_ATTRIBUTES_EVENT_CALLBACKS_INIT(&moduleAttributes,
                                               &moduleCallbacks);
    moduleCallbacks.EvtModuleOnDeviceNotificationPostOpen = CFUHidTransport_PostOpen_Callback;
    moduleCallbacks.EvtModuleOnDeviceNotificationPreClose = CFUHidTransport_PreClose_Callback;

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &deviceContext->DmfModuleComponentFirmwareUpdateTransportHid);

Exit:

    FuncExitVoid(TRACE_DEVICE);
}
#pragma code_seg()

// eof: DmfInterface.c
//