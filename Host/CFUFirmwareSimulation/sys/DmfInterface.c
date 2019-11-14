/*++

    Copyright (c) Microsoft Corporation. All rights reserved

Module Name:

    DmfInterface.c

Abstract:

    Instantiate Dmf Library Modules used by this driver.
    
Environment:

    kernel-mode Driver Framework

--*/
#include "Common.h"

#include "DmfInterface.tmh"

///////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE
///////////////////////////////////////////////////////////////////////////////////////////



// HID Report Descriptor for dmf PlatformPolicyManager.
//
static
const
UCHAR
g_CfuVirtualHid_HidReportDescriptor[] =
{
    0x06, CFU_DEVICE_USAGE_PAGE,        // USAGE_PAGE(0xFA00) 
    0x09, CFU_DEVICE_USAGE,             // USAGE(0xF5) 
    0xA1, 0x01,                         // COLLECTION(0x01)
    0x15, 0x00,                         // LOGICAL_MINIMUM(0)
    0x27, 0xFF, 0xFF, 0xFF, 0xFF,       // LOGICAL_MAXIMUM(-1)
    0x75, 0x08,                         // REPORT SIZE(8)

    0x85, REPORT_ID_VERSIONS_FEATURE,   // REPORT_ID(32)
    0x95, FEATURE_REPORT_LENGTH,        // REPORT COUNT(60)
    0x09, VERSIONS_FEATURE_USAGE,       // USAGE(0x42)
    0xB2, 0x02, 0x01,                   // FEATURE(0x02)

    0x85, REPORT_ID_PAYLOAD_OUTPUT,     // REPORT_ID(32)
    0x95, OUTPUT_REPORT_LENGTH,         // REPORT COUNT(60)
    0x09, PAYLOAD_OUTPUT_USAGE,         // USAGE(0x31)
    0x92, 0x02, 0x01,                   // OUTPUT(0x02)

    0x85, REPORT_ID_PAYLOAD_INPUT,      // REPORT_ID(34)
    0x27, 0xFF, 0xFF, 0xFF, 0xFF,       // LOGICAL_MAXIMUM(-1)
    0x75, INPUT_REPORT_LENGTH,          // REPORT SIZE(32)
    0x95, 0x04,                         // REPORT COUNT(4)
    0x19, PAYLOAD_INPUT_USAGE_MIN,      // USAGE MIN (0x26)
    0x29, PAYLOAD_INPUT_USAGE_MAX,      // USAGE MAX (0x29)
    0x81, 0x02,                         // INPUT(0x02)

    0x85, REPORT_ID_OFFER_INPUT,        // REPORT_ID(37)
    0x19, OFFER_INPUT_USAGE_MIN,        // USAGE MIN (0x1A)
    0x29, OFFER_INPUT_USAGE_MAX,        // USAGE MAX (0x1D)
    0x81, 0x02,                         // INPUT(0x02)

    0x85, REPORT_ID_OFFER_OUTPUT,       // REPORT_ID(37)
    0x19, OFFER_OUTPUT_USAGE_MIN,       // USAGE MIN (0x1E)
    0x29, OFFER_OUTPUT_USAGE_MAX,       // USAGE MAX (0x21)
    0x91, 0x02,                         // OUTPUT(0x02)

    0xC0                                // END_COLLECTION()
};

// HID Device Descriptor to expose dmf PlatformPolicyManager as a virtual HID device.
//
static
const
HID_DESCRIPTOR
g_CfuVirtualHid_HidDescriptor =
{
    0x09,     // Length of HID descriptor
    0x21,     // Descriptor type == HID  0x21
    0x0100,   // HID spec release
    0x00,     // Country code == English
    0x01,     // Number of HID class descriptors
    {
        0x22,   // Descriptor type
        // Total length of report descriptor.
        //
        (USHORT) sizeof(g_CfuVirtualHid_HidReportDescriptor)
    }
};


DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD CfuVirtualHidEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP CfuVirtualHidEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD CfuVirtualHidDeviceModulesAdd;

/* WPP_INIT_TRACING(); (Comment required by WPP parser)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        CfuVirtualHidEvtDriverContextCleanup,
                        CfuVirtualHidEvtDeviceAdd)
#pragma code_seg()

#pragma code_seg("PAGED")
    DMF_DEFAULT_DRIVERCLEANUP(CfuVirtualHidEvtDriverContextCleanup)
#pragma code_seg()

VOID
CfuDevice_ResponseThreadWork(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback function for Child DMF Module Thread.
    This consumes the command buffer and completes an input report.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    None

--*/
{

    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;

    NTSTATUS ntStatus;

    ULONG* responseBufferSize = NULL;
    VOID* clientBuffer = NULL;
    VOID* clientBufferContext = NULL;

    FuncEntry(TRACE_DEVICE);

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    //
    device = DMF_ParentDeviceGet(DmfModule);
    deviceContext = DeviceContextGet(device);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "CfuDevice_ResponseThreadWork");

    // Get a buffer from 'BufferPool'.
    //
    ntStatus = DMF_BufferQueue_Dequeue(deviceContext->DmfModuleResponseBufferQueue,
                                       &clientBuffer,
                                       &clientBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "DMF_BufferQueue_Fetch ntStatus=0x%x", ntStatus);
        goto Exit;
    }

    ASSERT(clientBuffer != NULL);
    ASSERT(clientBufferContext != NULL);

    RESPONSE_BUFFER* responseBuffer = (RESPONSE_BUFFER*)clientBuffer;
    responseBufferSize = (ULONG*)clientBufferContext;
    ASSERT(*responseBufferSize == sizeof(RESPONSE_BUFFER));

    // Process the response here.
    //
    ntStatus = CfuDevice_ResponseSend(deviceContext,
                                      responseBuffer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Worker Thread Response Processing fails ntStatus=0x%x", ntStatus);
        goto Exit;
    }

Exit:

    if (clientBuffer)
    {
        DMF_BufferQueue_Reuse(deviceContext->DmfModuleResponseBufferQueue,
                              clientBuffer);
    }

    FuncExitVoid(TRACE_DEVICE);
}

#pragma code_seg("PAGE")
_Function_class_(EVT_WDF_DRIVER_DEVICE_ADD)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
CfuVirtualHidEvtDeviceAdd(
    _In_    WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
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

    WDF_OBJECT_ATTRIBUTES attributes;
    PDMFDEVICE_INIT dmfDeviceInit;
    DMF_EVENT_CALLBACKS dmfCallbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    FuncEntry(TRACE_DEVICE);
    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = CfuDevice_EvtDevicePrepareHardware;
    pnp.EvtDeviceReleaseHardware = CfuDevice_EvtDeviceReleaseHardware;

    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit,
                                                &pnp);

    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, 
                                           &pnp);

    // All DMF drivers must call this function even if they do not support File Object callbacks.
    //
    DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit,
                                          NULL);
    // All DMF drivers must call this function even if they do not support Power Policy callbacks.
    //
    DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit,
                                                   NULL);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            DEVICE_CONTEXT);
    ntStatus = WdfDeviceCreate(&DeviceInit,
                               &attributes,
                               &device);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "WdfDeviceCreate fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    DMF_EVENT_CALLBACKS_INIT(&dmfCallbacks);
    dmfCallbacks.EvtDmfDeviceModulesAdd = CfuVirtualHidDeviceModulesAdd;
    DMF_DmfDeviceInitSetEventCallbacks(dmfDeviceInit,
                                       &dmfCallbacks);

    ntStatus = DMF_ModulesCreate(device,
                                 &dmfDeviceInit);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "DMF_ModulesCreate fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }
    
Exit:

    if (dmfDeviceInit != NULL)
    {
        DMF_DmfDeviceInitFree(&dmfDeviceInit);
    }

    FuncExit(TRACE_DEVICE, "ntStatus=%!STATUS!", ntStatus);
    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
CfuVirtualHidDeviceModulesAdd(
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
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_VirtualHidDeviceVhf virtualHidDeviceModuleConfig;
    DMF_CONFIG_Thread threadModuleConfig;
    DMF_CONFIG_BufferQueue bufferQueueModuleConfig;
    PDEVICE_CONTEXT deviceContext;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModuleInit);
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(TRACE_DEVICE);

    deviceContext = DeviceContextGet(Device);

    // VirtualHidDeviceVhf
    // -------------------
    //
    DMF_CONFIG_VirtualHidDeviceVhf_AND_ATTRIBUTES_INIT(&virtualHidDeviceModuleConfig,
                                                       &moduleAttributes);

    virtualHidDeviceModuleConfig.VendorId = VENDOR_ID;
    virtualHidDeviceModuleConfig.ProductId = PRODUCT_ID;
    virtualHidDeviceModuleConfig.VersionNumber = 0x0001;

    virtualHidDeviceModuleConfig.HidDescriptor = &g_CfuVirtualHid_HidDescriptor;
    virtualHidDeviceModuleConfig.HidDescriptorLength = sizeof(g_CfuVirtualHid_HidDescriptor);
    virtualHidDeviceModuleConfig.HidReportDescriptor = g_CfuVirtualHid_HidReportDescriptor;
    virtualHidDeviceModuleConfig.HidReportDescriptorLength = sizeof(g_CfuVirtualHid_HidReportDescriptor);

    // Set virtual device attributes.
    //
    virtualHidDeviceModuleConfig.HidDeviceAttributes.VendorID = VENDOR_ID;
    virtualHidDeviceModuleConfig.HidDeviceAttributes.ProductID = PRODUCT_ID;
    virtualHidDeviceModuleConfig.HidDeviceAttributes.VersionNumber = 0x0001;
    virtualHidDeviceModuleConfig.HidDeviceAttributes.Size = sizeof(virtualHidDeviceModuleConfig.HidDeviceAttributes);

    virtualHidDeviceModuleConfig.StartOnOpen = TRUE;
    virtualHidDeviceModuleConfig.VhfClientContext = Device;

    // Set callbacks from upper layer.
    //
    virtualHidDeviceModuleConfig.IoctlCallback_IOCTL_HID_WRITE_REPORT = CfuDevice_WriteReport;  //offer,payload send
    virtualHidDeviceModuleConfig.IoctlCallback_IOCTL_HID_GET_FEATURE  = CfuDevice_GetFeatureReport; //versions

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &deviceContext->DmfModuleVirtualHidDeviceVhf);

    // Thread
    // ------
    //
    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&threadModuleConfig,
                                          &moduleAttributes);
    threadModuleConfig.ThreadControlType = ThreadControlType_DmfControl;
    threadModuleConfig.ThreadControl.DmfControl.EvtThreadWork = CfuDevice_ResponseThreadWork;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &deviceContext->DmfModuleThread);

    // BufferQueue
    // -----------
    //
    DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&bufferQueueModuleConfig,
                                               &moduleAttributes);
    bufferQueueModuleConfig.SourceSettings.EnableLookAside = TRUE;
    bufferQueueModuleConfig.SourceSettings.BufferCount = 5;
    bufferQueueModuleConfig.SourceSettings.BufferSize = sizeof(RESPONSE_BUFFER);
    bufferQueueModuleConfig.SourceSettings.BufferContextSize = sizeof(ULONG);
    bufferQueueModuleConfig.SourceSettings.PoolType = NonPagedPoolNx;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &deviceContext->DmfModuleResponseBufferQueue);

    FuncExitVoid(TRACE_DEVICE);

}
#pragma code_seg()

// eof: DmfInterface.c
//