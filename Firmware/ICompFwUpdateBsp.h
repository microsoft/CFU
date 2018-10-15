/*++
    This file is part of Component Firmware Update (CFU), licensed under
    the MIT License (MIT).

    Copyright (c) Microsoft Corporation. All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

Module Name:

    ICompFwUpdateBsp.h

Abstract:

    Interface definition for platform bsp fwu support.

Environment:

    Firmware driver. 
--*/
#pragma once

//****************************************************************************
//
//                               INCLUDES
//
//****************************************************************************
#include "coretypes.h"
//****************************************************************************
//
//                                  DEFINES
//
//****************************************************************************

//****************************************************************************
//
//                                  TYPEDEFS
//
//****************************************************************************
typedef struct
{
    UINT32 (*Reader)         (UINT32 offset, UINT8* pData, UINT8 length);
    UINT32 (*Writer)         (UINT32 offset, UINT8* pData, UINT8 length);
    UINT32 (*Prepare)        (void);
} IEXTERNAL_STORAGE;
//****************************************************************************
//
//                          GLOBAL VARIABLE EXTERNS
//
//****************************************************************************

//****************************************************************************
//
//                          GLOBAL FUNCTION EXTERNS
//
//****************************************************************************

// Readers and writers for firmware update intermediates/self update handlers
UINT32 ICompFwUpdateBspPrepare(UINT8 componentId);
UINT32 ICompFwUpdateBspWrite(UINT32 offset, UINT8* pData, UINT8 length, UINT8 componentId);
UINT32 ICompFwUpdateBspRead(UINT32 offset, UINT8* pData, UINT16 length, UINT8 componentId);
// Component specific CRC calculator. 
UINT32 ICompFwUpdateBspCalcCRC(UINT16 *pCRC, UINT8 componentId);

// Signaled when internal MCU fwupdate is complete.
void ICompFwUpdateBspSignalUpdateComplete(void);

// If external storage is available on platform, it can be registered with this module.
void ICompFwUpdateBspRegisterExternalStorage(IEXTERNAL_STORAGE* pInterface);
