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
// Developer TODO - implement function to prepare memory to receive image.
//                  (NOTE: if image stored to flash/NVM, this is typically where
//                   the flash area is erased)
UINT32 ICompFwUpdateBspPrepare(UINT8 componentId);

// Developer TODO - implement function to write data chunk memory/flash.
UINT32 ICompFwUpdateBspWrite(UINT32 offset, UINT8* pData, UINT8 length, UINT8 componentId);

// Developer TODO - implement function to read data chunk from memory/flash.
UINT32 ICompFwUpdateBspRead(UINT32 offset, UINT8* pData, UINT16 length, UINT8 componentId);

// Developer TODO - implement function to calculate the CRC for the specific component image
UINT32 ICompFwUpdateBspCalcCRC(UINT16 *pCRC, UINT8 componentId);

// Developer TODO - implement function to perform authentication check for the specific component image
INT32 ICompFwUpdateBspAuthenticateFWImage(void);

// Developer TODO - implement function to perform any required functionality to let the 
//                  system know a new image has been downloaded and verified. (ex: this
//                  could be where the boot loader is modified to point to the new image )
void ICompFwUpdateBspSignalUpdateComplete(void);