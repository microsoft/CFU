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

    ComponentFwUpdate.h

Abstract:

    Header file for the component firmware update functionality..

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
#define CFU_OFFER_METADATA_INFO_CMD                        (0xFF)
#define CFU_SPECIAL_OFFER_CMD                              (0xFE)
#define CFU_SPECIAL_OFFER_GET_STATUS                       (0x03)
#define CFU_SPECIAL_OFFER_NONCE                            (0x02)
#define CFU_SPECIAL_OFFER_NOTIFY_ON_READY                  (0x01)
#define CFW_UPDATE_PACKET_MAX_LENGTH                       (sizeof(FWUPDATE_CONTENT_COMMAND))
#define FIRMWARE_OFFER_REJECT_BANK                         (0x04)
#define FIRMWARE_OFFER_REJECT_INV_MCU                      (0x01)
#define FIRMWARE_OFFER_REJECT_MISMATCH                     (0x03)
#define FIRMWARE_OFFER_REJECT_OLD_FW                       (0x00)
#define FIRMWARE_OFFER_TOKEN_DRIVER                        (0xA0)
#define FIRMWARE_OFFER_TOKEN_SPEEDFLASHER                  (0xB0)
#define FIRMWARE_UPDATE_CMD_NOT_SUPPORTED                  (0xFF)
#define FIRMWARE_UPDATE_FLAG_FIRST_BLOCK                   (0x80)
#define FIRMWARE_UPDATE_FLAG_LAST_BLOCK                    (0x40)
#define FIRMWARE_UPDATE_FLAG_VERIFY                        (0x08)
#define FIRMWARE_UPDATE_OFFER_ACCEPT                       (0x01)
#define FIRMWARE_UPDATE_OFFER_BUSY                         (0x03)
#define FIRMWARE_UPDATE_OFFER_COMMAND_READY                (0x04)
#define FIRMWARE_UPDATE_OFFER_REJECT                       (0x02)
#define FIRMWARE_UPDATE_OFFER_SKIP                         (0x00)
#define FIRMWARE_UPDATE_OFFER_SWAP_PENDING                 (0x02)
#define FIRMWARE_UPDATE_STATUS_ERROR_COMPLETE              (0x03)
#define FIRMWARE_UPDATE_STATUS_ERROR_CRC                   (0x05)
#define FIRMWARE_UPDATE_STATUS_ERROR_INVALID               (0x0B)
#define FIRMWARE_UPDATE_STATUS_ERROR_INVALID_ADDR          (0x09)
#define FIRMWARE_UPDATE_STATUS_ERROR_NO_OFFER              (0x0A)
#define FIRMWARE_UPDATE_STATUS_ERROR_PENDING               (0x08)
#define FIRMWARE_UPDATE_STATUS_ERROR_PREPARE               (0x01)
#define FIRMWARE_UPDATE_STATUS_ERROR_SIGNATURE             (0x06)
#define FIRMWARE_UPDATE_STATUS_ERROR_VERIFY                (0x04)
#define FIRMWARE_UPDATE_STATUS_ERROR_VERSION               (0x07)
#define FIRMWARE_UPDATE_STATUS_ERROR_WRITE                 (0x02)
#define FIRMWARE_UPDATE_STATUS_SUCCESS                     (0x00)
#define OFFER_INFO_END_OFFER_LIST                          (0x02)
#define OFFER_INFO_START_ENTIRE_TRANSACTION                (0x00)
#define OFFER_INFO_START_OFFER_LIST                        (0x01)
//****************************************************************************
//
//                                  TYPEDEFS
//
//****************************************************************************
// supress the warning about bit fields for only signed and unsigned ints
//#pragma diag_suppress=Pm095
#pragma pack(1)
typedef struct
{
    struct
    {
        UINT8 componentCount;
        UINT16 reserved0;
        UINT8 fwUpdateRevision : 4;
        UINT8 reserved1 : 3;
        UINT8 extensionFlag : 1;
    } header;
    UINT8 versionAndProductInfoBlob[20];
} GET_FWVERSION_RESPONSE;

typedef struct
{
    struct
    {
        UINT8 segmentNumber;
        UINT8 reserved0 : 6;
        UINT8 forceImmediateReset : 1;
        UINT8 forceIgnoreVersion : 1;
        UINT8 componentId;
        UINT8 token;
    } componentInfo;

    UINT32 version;
    UINT32 hwVariantMask;
    struct
    {
        UINT8 protocolRevision : 4;
        UINT8 bank : 2;
        UINT8 reserved0 : 2;
        UINT8 milestone : 3;
        UINT8 reserved1 : 5;
        UINT16 productId;
    } productInfo;

} FWUPDATE_OFFER_COMMAND;

typedef struct
{
    struct
    {
        UINT8 infoCode;
        UINT8 reserved0;
        UINT8 shouldBe0xFF;
        UINT8 token;
    } componentInfo;

    UINT32 reserved0[3];

} FWUPDATE_OFFER_INFO_ONLY_COMMAND;

typedef struct
{
    struct
    {
        UINT8 commandCode;
        UINT8 reserved0;
        UINT8 shouldBe0xFE;
        UINT8 token;
    } componentInfo;

    UINT32 reserved0[3];

} FWUPDATE_SPECIAL_OFFER_COMMAND;

typedef struct
{
    union
    {
        struct
        {
            UINT8 reserved0[3];
            UINT8 token;
            UINT32 reserved1;
            UINT8 rejectReasonCode;
            UINT8 reserved2[3];
            UINT8 status;
            UINT8 reserved3[3];
        };
    };
} FWUPDATE_OFFER_RESPONSE;

typedef struct
{
    UINT8 flags;
    UINT8 length;
    UINT16 sequenceNumber;
    UINT32 address;
    UINT8 pData[MAX_UINT8];
} FWUPDATE_CONTENT_COMMAND;

typedef struct
{
    union
    {
        struct
        {
            UINT16 sequenceNumber;
            UINT16 reserved0;
            UINT8 status;
            UINT8 reserved1[3];
            UINT32 reserved2[2];
        };
    };
} FWUPDATE_CONTENT_RESPONSE;

typedef enum
{
    BSP_YOURCOMPONENT      = (0x01U),
    // BSP Add more platform component ID's here
} FWUPDATE_PFID;

#pragma pack()
//#pragma diag_default=Pm095
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
void ProcessCFWUContent(FWUPDATE_CONTENT_COMMAND* pCommand, FWUPDATE_CONTENT_RESPONSE* pResponse);
void ProcessCFWUOffer(FWUPDATE_OFFER_COMMAND* pCommand, FWUPDATE_OFFER_RESPONSE* pResponse);
void ProcessCFWUGetFWVersion(GET_FWVERSION_RESPONSE* pResponse);
