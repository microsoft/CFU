/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Device.h

Abstract:

    Definition of device context for this driver

Environment:

    User-mode only.

--*/

#pragma once

typedef struct _DRIVER_FIRMWARE_INFORMATION
{
    WDFSTRING offerFileName;
    WDFSTRING payloadFileName;
    WDFMEMORY offerContentMemory;
    WDFMEMORY payloadContentMemory;
} DRIVER_FIRMWARE_INFORMATION, *PDRIVER_FIRMWARE_INFORMATION;

typedef enum _HID_TRANSPORT_PROTOCOL
{
    HID_TRANSPORT_PROTOCOL_INVALID,
    HID_TRANSPORT_PROTOCOL_USB,
    HID_TRANSPORT_PROTOCOL_BTLE,
    HID_TRANSPORT_PROTOCOL_MAXIMUM
} HID_TRANSPORT_PROTOCOL, *PHID_TRANSPORT_PROTOCOL;

typedef struct CFU_HID_TRANSPORT_CONIGURATION
{
    // Information about the underlying transport protocol for this module.
    //
    HID_TRANSPORT_PROTOCOL Protocol;
    // Number of Input reports reads that are simultaneously pended.
    //
    ULONG NumberOfInputReportReadsPended;
} CFU_HID_TRANSPORT_CONIGURATION, *PCFU_HID_TRANSPORT_CONIGURATION;

typedef struct CFU_PROTOCOL_CONIGURATION
{
    // Does this device support resuming a previously interrupted update?
    //
    BOOLEAN SupportResumeOnConnect;
    // Does this devie support skipping the entire protocol transaction for an already known all up-to-date Firmware?
    //
    BOOLEAN SupportProtocolTransactionSkipOptimization;
} CFU_PROTOCOL_CONIGURATION, *PCFU_PROTOCOL_CONIGURATION;

typedef struct _DEVICE_CONTEXT
{
    // Dmf Module Component Firmware Update.
    // 
    DMFMODULE DmfModuleComponentFirmwareUpdate;
    // Dmf Module Registry.
    // 
    DMFMODULE DmfModuleRegistry;
    // Collection of Firmware blob information read from registry.
    //
    WDFCOLLECTION FirmwareBlobCollection;
    // Configuration about the CFU Protocol.
    //
    CFU_PROTOCOL_CONIGURATION CfuProtocolConfiguration;
    // Configuration about the CFU Hid Transport.
    //
    CFU_HID_TRANSPORT_CONIGURATION CfuHidTransportConfiguration;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceContextGet)

#define MemoryTag 'dUWF'
#define EVENTLOG_PROVIDER_NAME L"SampleProvider"

// eof: Device.h
//