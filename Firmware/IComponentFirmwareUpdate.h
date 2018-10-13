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

    IComponentFimrwareUpdate.h

Abstract:

    Header for CFU Interface

Environment:

    Firmware driver. 
--*/

#pragma once

//****************************************************************************
//
//                               INCLUDES
//
//****************************************************************************
#include "McuStatus.h"
#include "ComponentFwUpdate.h"
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

typedef UINT32 (*READ_FIRMWARE_FUNC) (UINT32 offset, 
                                      UINT8* pData, 
                                      UINT16 length, 
                                      UINT8 componentId);
typedef void (*READ_COMPLETED_FUNC) (void);

typedef struct
{
    MCU_STATUS (*GetVersion)                (UINT32* pVersion);
    MCU_STATUS (*GetProductInfo)            (UINT32* pProductInfo);
    MCU_STATUS (*ProcessOffer)              (FWUPDATE_OFFER_COMMAND* pCommand, 
                                             FWUPDATE_OFFER_RESPONSE* pResponse);
    MCU_STATUS (*GetCrcOffset)              (UINT32* pOffset);  
    MCU_STATUS (*NotifySuccess)             (BOOL forceReset, 
                                             READ_FIRMWARE_FUNC readHandler, 
                                             READ_COMPLETED_FUNC readCompleteHandler);
} ICOMPONENT_INTERFACE;

typedef struct COMPONENT_REGISTRATION_STRUCT
{
    struct COMPONENT_REGISTRATION_STRUCT* pNext;
    const ICOMPONENT_INTERFACE interface;
    const UINT8 componentId;
} COMPONENT_REGISTRATION;

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
void IComponentFirmwareUpdateRegisterComponent(COMPONENT_REGISTRATION* pRegistration);
