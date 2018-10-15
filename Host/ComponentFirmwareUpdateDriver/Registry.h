/*++

    Copyright (C) Microsoft. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Registry.h

Abstract:

   Companion file to Registry.c.
    
Environment:

    User-mode Driver Framework

--*/

#pragma once

_Check_return_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_DeviceRegistryEnumerateAllFirwareSubKeys(
    _In_ WDFDEVICE Device
    );

// eof: Registry.h
//
