/*++

    Copyright (C) Microsoft. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Registry.c

Abstract:

   This file contains the registry related utilities.

Environment:

    User-mode Driver Framework

--*/
#include <intsafe.h>
#include "Common.h"

#include "Registry.tmh"

PCWSTR RegistryFirmwareRootValueName             = L"CFU";
PCWSTR RegistryHidTransportProtocolValueName     = L"Protocol";
PCWSTR RegistryHidNumberOfInputReportsValueName  = L"NumberOfInputReports";
PCWSTR RegistryProtocolResumeOnConnectValueName  = L"SupportResumeOnConnect";
PCWSTR RegistryProtocolSkipOptimizationValueName = L"SupportProtocolSkipOptimization";

// Define default values for configuration. 
// User can override them if needed.
//
const UINT NUMBER_OF_INPUT_REPORT_READ_SIMULTANEOUSLY_DEFAULT = 2;
const UINT ENABLE_RESUME_ON_CONNECT_FEATURE_DEFAULT = 0;
const UINT ENABLE_PROTOCOL_TRANSACTION_SKIP_OPTIMIZATION_FEATURE_DEFAULT = 0;
const HID_TRANSPORT_PROTOCOL HID_TRANSPORT_PROTOCOL_DEFAULT = HID_TRANSPORT_PROTOCOL_USB;

typedef
NTSTATUS
Registry_KeyEnumerationFunctionType(
    _In_ PVOID ClientContext,
    _In_ WDFKEY RootKey,
    _In_ PUNICODE_STRING KeyNameString
    );

_Check_return_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_SubKeysFromHandleEnumerate(
    _In_ WDFDEVICE Device,
    _In_ WDFKEY Key,
    _In_ Registry_KeyEnumerationFunctionType* RegistryEnumerationFunction
    )
/*++

Routine Description:

    Given a registry handle, enumerate all the sub keys and call an enumeration function for each of them.

Arguments:

    Device - WDF Device.
    Key - Key under which the enumerations are to be done.
    RegistryEnumerationFunction - The function that does the actual work with the enumerated registry node.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DWORD  numberOfSubKeys = 0;
    DWORD maxSubKeyLength = 0;
    HANDLE handle;
    WDFMEMORY subKeyNameMemory = NULL;

    FuncEntry(TRACE_DEVICE);
    
    // Sample Registry configuration.
    // HKR, CFU\MCU, Offer, 0x00000000, % 13 %\A.offer.bin
    // HKR, CFU\MCU, Payload, 0x00000000, % 13 % \A.srec.bin
    // HKR, CFU\SUB_COMPONENT0, Offer, 0x00000000, % 13 %\B.offer.bin
    // HKR, CFU\SUB_COMPONENT0, Payload, 0x00000000, % 13 %\B.srec.bin
    //
    // For the above example, Key passed in correspond to "CFU"
    //
    handle = WdfRegistryWdmGetHandle(Key);

    // Get the subkey count and maximum subkey name size.
    //
    ntStatus = RegQueryInfoKey((HKEY)handle,             // key handle 
                               NULL,                     // buffer for class name 
                               0,                        // size of class string 
                               NULL,                     // reserved 
                               &numberOfSubKeys,         // number of subkeys ()
                               &maxSubKeyLength,         // largest number of unicode chars
                               NULL,                     // longest class string 
                               NULL,                     // number of values for this key 
                               NULL,                     // longest value name 
                               NULL,                     // longest value data 
                               NULL,                     // security descriptor 
                               NULL);                    // last write time 
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] RegQueryInfoKey fails: ntStatus=%!STATUS!", 
                    Device, 
                    ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Number of subkeys:(0x%d)", 
                Device, 
                numberOfSubKeys);

    // Note: numberOfSubKey == 0 can be a valid case {for example the extension is not yet installed}.
    //
    if (numberOfSubKeys == 0)
    {
        // Report the warning in Event Log.
        //
        DMF_Utility_EventLogEntryWriteUserMode(EVENTLOG_PROVIDER_NAME,
                                               EVENTLOG_WARNING_TYPE,
                                               EVENTLOG_MESSAGE_NO_FIRMWARE_INFORMATION,
                                               0,
                                               NULL,
                                               0);
        goto Exit;
    }

    // Enumerate the subkeys.
    //
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = Device;

    LPTSTR subKeyNameMemoryBuffer = NULL;
    DWORD elementCountOfSubKeyName;
    WDFKEY firmwareInformationSubkey;

    // Create a buffer which is big enough to hold the largest subkey.
    // Account for NULL as well. Note: No overflow check as the registry key length maximum is limited.
    //
    elementCountOfSubKeyName = maxSubKeyLength + 1;
    size_t maxBytesRequired = (elementCountOfSubKeyName * sizeof(TCHAR));
    ntStatus = WdfMemoryCreate(&objectAttributes, 
                                PagedPool, 
                                MemoryTag, 
                                maxBytesRequired,
                                &subKeyNameMemory,
                                (PVOID*)&subKeyNameMemoryBuffer);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfMemoryCreate fails: ntStatus=%!STATUS!", 
                    Device, 
                    ntStatus);
        goto Exit;
    }

    for (DWORD keyIndex = 0; keyIndex<numberOfSubKeys; keyIndex++)
    {
        elementCountOfSubKeyName = maxSubKeyLength + 1;
        ZeroMemory(subKeyNameMemoryBuffer,
                    maxBytesRequired);

        // Read the subkey.
        // In the above example, the SubKeyName will be "MCU", "SUB_COMPONENT0" etc.
        //
        ntStatus = RegEnumKeyEx((HKEY)handle,
                                keyIndex,
                                subKeyNameMemoryBuffer,
                                (LPDWORD) &elementCountOfSubKeyName,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        TRACE_DEVICE, 
                        "[Device: 0x%p] RegEnumKeyEx fails: ntStatus=%!STATUS!", 
                        Device, 
                        ntStatus );
            break;
        }

        // Open the subkey and call the enumeration function to extract information under it.
        //
        UNICODE_STRING firmwareInformationSubkeyName;
        RtlInitUnicodeString(&firmwareInformationSubkeyName,
                             subKeyNameMemoryBuffer);

        ntStatus = WdfRegistryOpenKey(Key,
                                      &firmwareInformationSubkeyName,
                                      KEY_READ,
                                      &objectAttributes,
                                      &firmwareInformationSubkey);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
                        "[Device: 0x%p] RegOpenKeyEx fails to open (%S) ntStatus=%!STATUS!", 
                        Device, 
                        subKeyNameMemoryBuffer, 
                        ntStatus);
            break;
        }

        // Call enumeration function to read the firmware information.
        // For the example above, read 
        //        Offer, 0x00000000, % 13 %\A.offer.bin
        //        Payload, 0x00000000, % 13 %\A.srec.bin
        // etc.
        //
        ntStatus = RegistryEnumerationFunction(Device,
                                               firmwareInformationSubkey,
                                               &firmwareInformationSubkeyName);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        TRACE_DEVICE, 
                        "[Device: 0x%p] RegistryEnumerationFunction fails: ntStatus=%!STATUS!", 
                        Device, 
                        ntStatus);
            // continue 
            //
            ntStatus = STATUS_SUCCESS;
        }

        WdfRegistryClose(firmwareInformationSubkey);
    }

Exit:

    if (subKeyNameMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(subKeyNameMemory);
    }

    FuncExit(TRACE_DRIVER, "ntStatus=%!STATUS!", ntStatus);
    return ntStatus;
}

_Check_return_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_DeviceConfigurationGet(
    _In_ WDFDEVICE Device,
    _In_ WDFKEY RegKey 
    )
/*++

Routine Description:

    Read all the static configuration such as Hid Transport configuration and Protocol Configurations from registry 

Arguments:

    Device - WDF Device.
    RegKey - Key underwhich the information is present.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    UNICODE_STRING deviceInformationValueName;
    ULONG numberOfInputReports;
    ULONG supportResumeOnConnect;
    ULONG supportProtocolSkipOptimization;
    ULONG protocol;

    RtlInitUnicodeString(&deviceInformationValueName,
                         RegistryHidNumberOfInputReportsValueName);
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Reading %S ", 
                Device, 
                deviceInformationValueName.Buffer);

    ntStatus = WdfRegistryQueryULong(RegKey,
                                     &deviceInformationValueName,
                                     &numberOfInputReports);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfRegistryQueryULong fails to read %S ntStatus=%!STATUS! Using default %d", 
                    Device, 
                    deviceInformationValueName.Buffer, 
                    ntStatus,
                    NUMBER_OF_INPUT_REPORT_READ_SIMULTANEOUSLY_DEFAULT);

        numberOfInputReports = NUMBER_OF_INPUT_REPORT_READ_SIMULTANEOUSLY_DEFAULT;
        ntStatus = STATUS_SUCCESS;
    }

    RtlInitUnicodeString(&deviceInformationValueName,
                         RegistryProtocolResumeOnConnectValueName);
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Reading %S ", 
                Device, 
                deviceInformationValueName.Buffer);

    ntStatus = WdfRegistryQueryULong(RegKey,
                                     &deviceInformationValueName,
                                     &supportResumeOnConnect);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfRegistryQueryULong fails to read %S ntStatus=%!STATUS! Using Default %d", 
                    Device, 
                    deviceInformationValueName.Buffer, 
                    ntStatus,
                    ENABLE_RESUME_ON_CONNECT_FEATURE_DEFAULT);

        supportResumeOnConnect = ENABLE_RESUME_ON_CONNECT_FEATURE_DEFAULT;
        ntStatus = STATUS_SUCCESS;
    }

    RtlInitUnicodeString(&deviceInformationValueName,
                         RegistryProtocolSkipOptimizationValueName);
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Reading %S ", 
                Device, 
                deviceInformationValueName.Buffer);

    ntStatus = WdfRegistryQueryULong(RegKey,
                                     &deviceInformationValueName,
                                     &supportProtocolSkipOptimization);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfRegistryQueryULong fails to read %S ntStatus=%!STATUS! Using Default %d", 
                    Device, 
                    deviceInformationValueName.Buffer, 
                    ntStatus,
                    ENABLE_PROTOCOL_TRANSACTION_SKIP_OPTIMIZATION_FEATURE_DEFAULT);

        supportProtocolSkipOptimization = ENABLE_PROTOCOL_TRANSACTION_SKIP_OPTIMIZATION_FEATURE_DEFAULT;
        ntStatus = STATUS_SUCCESS;
    }

    RtlInitUnicodeString(&deviceInformationValueName,
                         RegistryHidTransportProtocolValueName);
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Reading %S ", 
                Device, 
                deviceInformationValueName.Buffer);

    ntStatus = WdfRegistryQueryULong(RegKey,
                                     &deviceInformationValueName,
                                     &protocol);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfRegistryQueryULong fails to read %S ntStatus=%!STATUS! Using Default %d", 
                    Device, 
                    deviceInformationValueName.Buffer, 
                    ntStatus,
                    HID_TRANSPORT_PROTOCOL_DEFAULT);

        protocol = HID_TRANSPORT_PROTOCOL_DEFAULT;
        ntStatus = STATUS_SUCCESS;
    }

    if (protocol <= HID_TRANSPORT_PROTOCOL_INVALID ||
        protocol >= HID_TRANSPORT_PROTOCOL_MAXIMUM)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfRegistryQueryULong %S Invalid Protocol Value %d", 
                    Device, 
                    deviceInformationValueName.Buffer, 
                    protocol);
        goto Exit;
    }


    PDEVICE_CONTEXT deviceContext = DeviceContextGet(Device);
    deviceContext->CfuHidTransportConfiguration.Protocol = (HID_TRANSPORT_PROTOCOL)protocol;
    deviceContext->CfuHidTransportConfiguration.NumberOfInputReportReadsPended = numberOfInputReports;

    deviceContext->CfuProtocolConfiguration.SupportResumeOnConnect = (supportResumeOnConnect>0);
    deviceContext->CfuProtocolConfiguration.SupportProtocolTransactionSkipOptimization = (supportProtocolSkipOptimization>0);

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] NumberOfInputReports 0x%x ", 
                Device, 
                deviceContext->CfuHidTransportConfiguration.NumberOfInputReportReadsPended);
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Protocol 0x%x ", 
                Device, 
                deviceContext->CfuHidTransportConfiguration.Protocol);
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Resume On Connect Support 0x%x ", 
                Device, 
                deviceContext->CfuProtocolConfiguration.SupportResumeOnConnect);
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Protocol Skip Optimization Support 0x%x ", 
                Device, 
                deviceContext->CfuProtocolConfiguration.SupportProtocolTransactionSkipOptimization);

Exit:

    return ntStatus;
}

_Check_return_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
Registry_FirmwareInformationEnumerate (
    _In_ PVOID ClientContext,
    _In_ WDFKEY RootKey,
    _In_ PUNICODE_STRING KeyNameString
    )
/*++

Routine Description:

    This is a callback function for the registry enumeration function. 
    Purpose of the callback is to extract filename information from registry for the offer and payload.

Arguments:

    ClientContext - Context provided by client (WDFDevice).
    RootKey - Regitry Root Key for which values are to be read by this function.
    KeyNameString - Registry Name. This is used only for informational purpose.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;

#define RegistryOfferValueName L"Offer"
#define RegistryPayloadValueName  L"Payload"

    DECLARE_CONST_UNICODE_STRING(offerRegValueName, RegistryOfferValueName);
    DECLARE_CONST_UNICODE_STRING(payloadRegValueName, RegistryPayloadValueName);
    PDRIVER_FIRMWARE_INFORMATION firmwareInformation;
    WDFSTRING offerFileNameObj;
    WDFSTRING payloadFileNameObj;
    UNICODE_STRING offerFileName;
    UNICODE_STRING payloadFileName;
    WDF_OBJECT_ATTRIBUTES attributes;

    device = (WDFDEVICE)ClientContext;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device;
    ntStatus = WdfStringCreate(NULL,
                               &attributes,
                               &offerFileNameObj);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfStringCreate fails: ntStatus=%!STATUS!", 
                    device, 
                    ntStatus);
        goto Exit;
    }

    ntStatus = WdfStringCreate(NULL,
                               &attributes,
                               &payloadFileNameObj);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfStringCreate fails: ntStatus=%!STATUS!", 
                    device, 
                    ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Reading %S/%S ", 
                device, 
                KeyNameString->Buffer, 
                offerRegValueName.Buffer);

    ntStatus = WdfRegistryQueryString(RootKey, 
                                      &offerRegValueName,
                                      offerFileNameObj);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfRegistryQueryString fails: to open %S ntStatus=%!STATUS!", 
                    device, 
                    offerRegValueName.Buffer, 
                    ntStatus);
        goto Exit;
    }
    WdfStringGetUnicodeString(offerFileNameObj, 
                              &offerFileName);
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Offer File is %S ", 
                device, 
                offerFileName.Buffer);

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE,
                "[Device: 0x%p] Reading %S/%S ", 
                device, 
                KeyNameString->Buffer, 
                payloadRegValueName.Buffer);
    ntStatus = WdfRegistryQueryString(RootKey,
                                      &payloadRegValueName,
                                      payloadFileNameObj);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, "[Device: 0x%p] WdfRegistryQueryString fails: to Open %S ntStatus=%!STATUS!", 
                    device, 
                    payloadRegValueName.Buffer, 
                    ntStatus);
        goto Exit;
    }

    WdfStringGetUnicodeString(payloadFileNameObj,
                              &payloadFileName);
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Payload File is %S ", 
                device, 
                payloadFileName.Buffer);

    deviceContext = DeviceContextGet(device);

    WDFMEMORY firmwareMemory;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = deviceContext->FirmwareBlobCollection;
    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeof(DRIVER_FIRMWARE_INFORMATION),
                               &firmwareMemory,
                               (PVOID*)&firmwareInformation);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfMemoryCreate for Firmware failed - %!STATUS!", 
                    device, 
                    ntStatus);
        goto Exit;
    }

    ZeroMemory(firmwareInformation, 
               sizeof(DRIVER_FIRMWARE_INFORMATION));

    // Update the information collected.
    // We will do lazy file read.
    //
    firmwareInformation->OfferFileName = offerFileNameObj;
    firmwareInformation->PayloadFileName = payloadFileNameObj;
    firmwareInformation->OfferContentMemory = WDF_NO_HANDLE;
    firmwareInformation->PayloadContentMemory = WDF_NO_HANDLE;

    ntStatus = WdfCollectionAdd(deviceContext->FirmwareBlobCollection, 
                                firmwareMemory);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfCollectionAdd for firmware failed - %!STATUS!", 
                    device, 
                    ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}

_Check_return_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_DeviceRegistryEnumerateAllFirwareSubKeys(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Enumerate all subkey under DeviceHardware key and Retrieve the firmware information.

Arguments:

    Device - WDF Device.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFKEY deviceHardwareKey = NULL;
    WDFKEY firmwareInformationRootKey = NULL;
    UNICODE_STRING firmwareInformationRootKeyStr;

    FuncEntry(TRACE_DEVICE);

    if (Device == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "Invalid argument: device(0x%p) ", 
                    Device);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Open the device Hardware Key for read.
    //
    ntStatus = WdfDeviceOpenRegistryKey(Device, 
                                        PLUGPLAY_REGKEY_DEVICE, 
                                        KEY_READ, 
                                        WDF_NO_OBJECT_ATTRIBUTES, 
                                        &deviceHardwareKey);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfDeviceOpenRegistryKey fails to open driver's software key ntStauts=%!STATUS!", 
                    Device, 
                    ntStatus);
        goto Exit;
    }

    // Retrieve the hid transport and protocol configuration from device hardware key.
    //
    ntStatus = Registry_DeviceConfigurationGet(Device,
                                               deviceHardwareKey);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] Registry_DeviceConfigurationGet fails to retrieve device specific information ntStauts=%!STATUS!", 
                    Device, 
                    ntStatus);

        // Report the warning in Event Log.
        //
        DMF_Utility_EventLogEntryWriteUserMode(EVENTLOG_PROVIDER_NAME,
                                               EVENTLOG_WARNING_TYPE,
                                               EVENTLOG_MESSAGE_NO_PROTOCOL_OR_TRANSPORT_INFORMATION,
                                               0,
                                               NULL,
                                               0);
        goto Exit;
    }

    // Enumate all the Firmware information under the firmare information root key "CFU".
    // E.g.
    // HKR, CFU\MCU, Offer, 0x00000000, % 13 %\A.offer.bin
    // HKR, CFU\MCU, Payload, 0x00000000, % 13 % \A.srec.bin
    // HKR, CFU\SUB_COMPONENT0, Offer, 0x00000000, % 13 % \B.offer.bin
    // HKR, CFU\SUB_COMPONENT0, Payload, 0x00000000, % 13 % \B.srec.bin
    //
    // Open the firmware information root key.
    //
    RtlInitUnicodeString(&firmwareInformationRootKeyStr,
                         RegistryFirmwareRootValueName);

    ntStatus = WdfRegistryOpenKey(deviceHardwareKey,
                                  &firmwareInformationRootKeyStr,
                                  KEY_READ,
                                  WDF_NO_OBJECT_ATTRIBUTES,
                                  &firmwareInformationRootKey);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfRegistryOpenKey fails to open FirmwareInformation Root Key ntStauts=%!STATUS!", 
                    Device, 
                    ntStatus);
        goto Exit;
    }

    // Enumerate and collect all the firmware information.
    //
    ntStatus = Registry_SubKeysFromHandleEnumerate(Device,
                                                   firmwareInformationRootKey,
                                                   Registry_FirmwareInformationEnumerate);

Exit:

    if (deviceHardwareKey != NULL)
    {
        WdfRegistryClose(deviceHardwareKey);
        deviceHardwareKey = NULL;
    }

    if (firmwareInformationRootKey != NULL)
    {
        WdfRegistryClose(firmwareInformationRootKey);
        firmwareInformationRootKey = NULL;
    }

    FuncExit(TRACE_DEVICE, "%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Registry.c
//
