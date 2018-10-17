/*++

    Copyright (C) Microsoft. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Firmware.h

Abstract:


   Companion file to Firmware.c.

    
Environment:

    User-mode Driver Framework

--*/
#pragma once

_Must_inspect_result_
NTSTATUS
ComponentFirmwareUpdateOfferGet(
    _In_ DMFMODULE DmfModule,
    _In_ DWORD FirmwarePairIndex,
    _Out_ BYTE** FirmwareBuffer,
    _Out_ size_t* BufferLength
    );

_Must_inspect_result_
NTSTATUS
ComponentFirmwareUpdatePayloadGet(
    _In_ DMFMODULE DmfModule,
    _In_ DWORD FirmwarePairIndex,
    _Out_ BYTE** FirmwareBuffer,
    _Out_ size_t* BufferLength
    );

// eof: Firmware.h
//