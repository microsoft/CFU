/*++

    Copyright (C) Microsoft. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Firmware.c

Abstract:

   This file contains the firmware file (offer/payload) related functions.

Environment:

    User-mode Driver Framework

--*/

#include "Common.h"

#include "Firmware.tmh"

_Must_inspect_result_
static
BOOL 
Firmware_FileContentRead(
    _In_ WDFDEVICE Device,
    _In_ WDFSTRING FileName, 
    _Out_ WDFMEMORY* FileContentMemory
    )
/*++

Routine Description:

    This is helper function creates a buffer and reads the file contents into it.

Arguments:

    Device - WDF Device.
    FileName - Name of the file.
    FileContentMemory - WDFMemory where the contents are copied available. The caller owns this memory.

Return Value:

    BOOL- TRUE if the file is read successfully and copied, FALSE otherwise.

--*/
{
    NTSTATUS ntStatus;
    PDEVICE_CONTEXT deviceContext;
    BOOL returnValue;
    LONGLONG bytesRemaining;
    PBYTE readBuffer;
    DWORD sizeOfOneRead;
    DWORD numberOfBytesRead;
    LARGE_INTEGER fileSize;
    UNICODE_STRING fileNameString;
    WDFMEMORY firmwareMemory;

    WdfStringGetUnicodeString(FileName,
                              &fileNameString);

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                TRACE_DEVICE, 
                "[Device: 0x%p] Reading Firmware file %S ",
                Device,
                fileNameString.Buffer);

    returnValue = FALSE;
    firmwareMemory = WDF_NO_HANDLE;
    *FileContentMemory = WDF_NO_HANDLE;

    HANDLE hFile = CreateFile(fileNameString.Buffer,  // name of file to open
                              GENERIC_READ,           // open for reading
                              FILE_SHARE_READ,        // shared read
                              NULL,                   // default security
                              OPEN_EXISTING,          // open existing file only
                              FILE_ATTRIBUTE_NORMAL,  // normal file
                              NULL);                  // no attr. template
    if (hFile == INVALID_HANDLE_VALUE)
    {
        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        TraceError(TRACE_DEVICE,
                   "[Device: 0x%p] CreateFile fails: to Open %S! ntStatus=%!STATUS!",
                   Device,
                   fileNameString.Buffer,
                   ntStatus);
        goto Exit;
    }

    fileSize.QuadPart = 0;
    returnValue = GetFileSizeEx(hFile,
                                &fileSize);
    if (!returnValue)
    {
        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        TraceError(TRACE_DEVICE,
                   "[Device: 0x%p] GetFileSizeEx fails: to Read %S !ntStatus=%!STATUS!", 
                   Device,
                   fileNameString.Buffer,
                   ntStatus);
        goto Exit;
    }

    deviceContext = DeviceContextGet(Device);

    // Allocate the required buffer to read the file content.
    //
    PBYTE fileContentBuffer;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = deviceContext->FirmwareBlobCollection;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               (size_t)fileSize.QuadPart,
                               &firmwareMemory,
                               &fileContentBuffer);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    TRACE_DEVICE, 
                    "[Device: 0x%p] WdfMemoryCreate fails: ntStatus=%!STATUS!", 
                    Device, 
                    ntStatus);
        goto Exit;
    }

    // Now read the contents.
    //
    bytesRemaining = fileSize.QuadPart;
    readBuffer = fileContentBuffer;
    while (bytesRemaining > 0)
    {
        numberOfBytesRead = 0;
        sizeOfOneRead = MAXDWORD;
        if (bytesRemaining < sizeOfOneRead)
        {
            sizeOfOneRead = (DWORD) bytesRemaining;
        }

        returnValue = ReadFile(hFile,
                               readBuffer,
                               sizeOfOneRead,
                               &numberOfBytesRead,
                               NULL);
        if (!returnValue)
        {
            ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
            TraceError(TRACE_DEVICE,
                       "[Device: 0x%p] ReadFile fails: to Read %S !ntStatus=%!STATUS!", 
                       Device,
                       fileNameString.Buffer,
                       ntStatus);
            break;
        }

        readBuffer += numberOfBytesRead;
        bytesRemaining -= numberOfBytesRead;

        if (numberOfBytesRead == 0)
        {
            ASSERT(bytesRemaining == 0);
        }
    }

    *FileContentMemory = firmwareMemory;

Exit:

    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }

    return returnValue;
}

_Must_inspect_result_
NTSTATUS
ComponentFirmwareUpdateOfferGet(
    _In_ DMFMODULE DmfModule,
    _In_ DWORD FirmwarePairIndex,
    _Out_ BYTE** FirmwareBuffer,
    _Out_ size_t* FirmwareBufferLength
    )
/*++

Routine Description:

    This is function reads returns contents of offer file at the specified index. 
    This function is called by the CFU protocol module when it is ready to process a firmware offer content.

Arguments:

    DmfModule - Dmf Handle to Protocol Module.
    FirmwarePairIndex - Index of the offer file for which the contents is sought.
    FirmwareBuffer - Buffer returned to the caller.
    FirmwareBufferLength - size in bytes of the buffer returned to the caller.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;
    ULONG numberOfFirmwarePairs;

    device = DMF_ParentDeviceGet(DmfModule);
    deviceContext = DeviceContextGet(device);

    ASSERT(FirmwareBufferLength != NULL);

    numberOfFirmwarePairs = WdfCollectionGetCount(deviceContext->FirmwareBlobCollection);
    ASSERT(FirmwarePairIndex < numberOfFirmwarePairs);

    WDFMEMORY firmwareInformationMemory = (WDFMEMORY)WdfCollectionGetItem(deviceContext->FirmwareBlobCollection,
                                                                          FirmwarePairIndex);

    PDRIVER_FIRMWARE_INFORMATION firmwareInformation = (PDRIVER_FIRMWARE_INFORMATION)WdfMemoryGetBuffer(firmwareInformationMemory,
                                                                                                        NULL);
    if (firmwareInformation->OfferContentMemory == WDF_NO_HANDLE)
    {
        BOOL returnVal = Firmware_FileContentRead(device,
                                                  firmwareInformation->OfferFileName,
                                                  &firmwareInformation->OfferContentMemory);
        if (!returnVal)
        {
            ntStatus = STATUS_FILE_NOT_AVAILABLE;
            goto Exit;
        }
    }

    PBYTE offerBuffer;
    size_t offerBufferSize;
    offerBuffer = (PBYTE) WdfMemoryGetBuffer(firmwareInformation->OfferContentMemory,
                                             &offerBufferSize);
    *FirmwareBuffer = offerBuffer;
    *FirmwareBufferLength = offerBufferSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_Must_inspect_result_
NTSTATUS
ComponentFirmwareUpdatePayloadGet(
    _In_ DMFMODULE DmfModule,
    _In_ DWORD FirmwarePairIndex,
    _Out_ BYTE** FirmwareBuffer,
    _Out_ size_t* FirmwareBufferSize
    )
/*++

Routine Description:

    This is function reads and returns contents of payload file at the specified index. 
    This function is called by the CFU protocol module when it is ready to process a firmware payload content.

Arguments:

    DmfModule - Dmf Handle to Protocol Module.
    FirmwarePairIndex - Index of the payload file for which the contents is sought.
    FirmwareBuffer - Buffer returned to the caller.
    FirmwareBufferLength - size in bytes of the buffer returned to the caller.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;
    ULONG numberOfFirmwarePairs;

    device = DMF_ParentDeviceGet(DmfModule);
    deviceContext = DeviceContextGet(device);

    ASSERT(FirmwareBufferSize != NULL);

    numberOfFirmwarePairs = WdfCollectionGetCount(deviceContext->FirmwareBlobCollection);
    ASSERT(FirmwarePairIndex < numberOfFirmwarePairs);

    WDFMEMORY firmwareInformationMemory = (WDFMEMORY)WdfCollectionGetItem(deviceContext->FirmwareBlobCollection,
                                                                          FirmwarePairIndex);

    PDRIVER_FIRMWARE_INFORMATION firmwareInformation = (PDRIVER_FIRMWARE_INFORMATION)WdfMemoryGetBuffer(firmwareInformationMemory,
                                                                                                        NULL);
    if (firmwareInformation->PayloadContentMemory == WDF_NO_HANDLE)
    {
        BOOL returnVal = Firmware_FileContentRead(device,
                                                  firmwareInformation->PayloadFileName,
                                                  &firmwareInformation->PayloadContentMemory);
        if (!returnVal)
        {
            ntStatus = STATUS_FILE_NOT_AVAILABLE;
            goto Exit;
        }
    }

    PBYTE payloadBuffer;
    size_t payloadBufferSize;
    payloadBuffer = (PBYTE) WdfMemoryGetBuffer(firmwareInformation->PayloadContentMemory,
                                               &payloadBufferSize);
    *FirmwareBuffer = payloadBuffer;
    *FirmwareBufferSize = payloadBufferSize;
    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

// eof: Firmware.c
//