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

    ComponentFwUpdate.c

Abstract:

    Implementation of the component firmware update protocol.

Environment:

    Firmware driver. 
--*/
//****************************************************************************
//
//                               INCLUDES
//
//****************************************************************************
#include <stdlib.h>
#include "inc\coretypes.h"
#include "inc\ComponentFwUpdate.h"
#include "inc\ICompFwUpdateBsp.h"
#include "inc\IComponentFirmwareUpdate.h"
#include "inc\FwVersion.h"

#define CPFWU_REVISION  (2u)

//****************************************************************************
//
//                                  DEFINES
//
//****************************************************************************
// BSP Modify Timer functionality to meet your platform needs.
typedef UINT16 TIMER_ID; // change this to your needs for timer definition
TIMER_ID BSP_Timer_Create ( void (* pTimerCallback)(void), UINT32 timeoutMs){return (TIMER_ID) 1;}
void BSP_Timer_Stop(TIMER_ID timerId){}
void BSP_Timer_Restart(TIMER_ID timerId){}
// /BSP  

// Maximum time for a single image update to finish.
//   if it takes longer than this - the CFU engine is reset.
#define MAX_FW_UPDATE_TIME_FAIL_SAFE_MS         (20 * 60 * 1000)

//****************************************************************************
//
//                                  TYPEDEFS
//
//****************************************************************************
typedef struct
{
    UINT8   activeComponentId;
    BOOL    forceReset;
    BOOL    updateInProgress;
} CURRENT_OFFER_INFO;

//****************************************************************************
//
//                              STATIC VARIABLES
//
//****************************************************************************
static CURRENT_OFFER_INFO       s_currentOffer;
static COMPONENT_REGISTRATION*  s_pFirstComponentIFace = NULL;
static TIMER_ID                 s_updateTimer = 0; //BSP Modify initial value 
                                                   // to your platform needs
static BOOL                     s_bankSwapPending = FALSE;
//****************************************************************************
//
//                          STATIC FUNCTION PROTOTYPES
//
//****************************************************************************
static void _ReadCompleteCallback(void);
static void _UpdateTimerCallback(void);
static UINT32 FirmwareUpdateInit(void);
//****************************************************************************
//
//                              GLOBAL VARIABLES
//
//****************************************************************************

//****************************************************************************
//
//                              FUNCTION CODE
//
//****************************************************************************

//****************************************************************************
//
// _ReadCompleteCallback - Callback when component is done with image.
//
//****************************************************************************
static void _ReadCompleteCallback(void)
{
    s_currentOffer.updateInProgress = FALSE;
}

static void _UpdateTimerCallback(void)
{
    s_currentOffer.updateInProgress = FALSE;
    BSP_Timer_Stop(s_updateTimer);
}

//****************************************************************************
//
//                              GLOBAL FUNCTIONS
//
//****************************************************************************

//****************************************************************************
//
// FirmwareUpdateInit - Initialization that needs to occur when its 
// task starts up
// 
//    NOTE: This function must be called at system start up
//
//****************************************************************************
UINT32 FirmwareUpdateInit(void)
{
    s_updateTimer = BSP_Timer_Create( _UpdateTimerCallback, 
                                      MAX_FW_UPDATE_TIME_FAIL_SAFE_MS);
    BSP_Timer_Stop(s_updateTimer);
    return 0;
}

//******************************************************************************
//
// ProcessCFWUContent - Process the content component firmware update command.
//
// Input Parameters
//      FWUPDATE_CONTENT_COMMAND* pCommand - The command to process.
//      FWUPDATE_CONTENT_RESPONSE* pResponse - The response to populate.
//
//******************************************************************************
void ProcessCFWUContent(FWUPDATE_CONTENT_COMMAND* pCommand, 
        FWUPDATE_CONTENT_RESPONSE* pResponse)
{
    UINT8 status = FIRMWARE_UPDATE_STATUS_SUCCESS;
    UINT16 sequenceNumber = pCommand->sequenceNumber;
    UINT8 componentId = s_currentOffer.activeComponentId;

    if (pCommand->flags & FIRMWARE_UPDATE_FLAG_FIRST_BLOCK)
    {
        // FWU: Received first block flag, starting FWupdate.

        if (ICompFwUpdateBspPrepare(componentId) == 0)
        {
            if (ICompFwUpdateBspWrite(pCommand->address, pCommand->pData, 
                        pCommand->length, componentId) != 0)
            {
                status = FIRMWARE_UPDATE_STATUS_ERROR_WRITE;
            }
        }
        else
        {
            status = FIRMWARE_UPDATE_STATUS_ERROR_PREPARE;
        }

    }
    else if (pCommand->flags & FIRMWARE_UPDATE_FLAG_LAST_BLOCK)
    {
        if (ICompFwUpdateBspWrite(pCommand->address, pCommand->pData, 
                    pCommand->length, componentId) == 0)
        {
            COMPONENT_REGISTRATION* pRegistration = s_pFirstComponentIFace;

            MCU_STATUS getCrcOffsetResult = MCU_STATUS_DEFAULT_ERROR;
            UINT32 crcOffset = 0;

            while (pRegistration)
            {
                if (pRegistration->componentId == componentId)
                {
                    // Found a matching componentId, Get the CRC
                    getCrcOffsetResult = pRegistration->interface.GetCrcOffset(&crcOffset);
                    break;
                }

                pRegistration = pRegistration->pNext;
            }

            if (!MCU_SUCCESS(getCrcOffsetResult))
            {
                // Error retrieving crc offset
                status = FIRMWARE_UPDATE_STATUS_ERROR_INVALID;
            }
            else if (getCrcOffsetResult != MCU_STATUS_NVM_CRC_CHECK_NOT_REQUIRED)
            {
                // CRC check required
                UINT16 crc;
                UINT16 calculatedCrc;

                if (ICompFwUpdateBspCalcCRC(&calculatedCrc,  componentId) != 0)
                {
                    status = FIRMWARE_UPDATE_STATUS_ERROR_CRC;
                }
                else if (ICompFwUpdateBspRead(crcOffset, (UINT8*)&crc, sizeof(crc), componentId) != 0)
                {
                    status = FIRMWARE_UPDATE_STATUS_ERROR_CRC;
                }
                else if (crc != calculatedCrc)
                {
                    status = FIRMWARE_UPDATE_STATUS_ERROR_CRC;
                }
                else
                {
                    //Successfully validated CRC
                    
                    //Perform any other image verification here
                    //  ex. Signatures, cert, encryption/decryption etc
                }
            }
            else
            {
                // Skipping CRC check, notifying firmware ready
                //OR Perform any other image verification here
                //  ex. Signatures, cert, encryption/decryption etc
            }

            if (status == FIRMWARE_UPDATE_STATUS_SUCCESS)
            {
                if (MCU_SUCCESS(pRegistration->
                            interface.NotifySuccess(s_currentOffer.forceReset, 
                                ICompFwUpdateBspRead, _ReadCompleteCallback)))
                {
                    // Final component specific step of image consumption has succeeded
                    // We have successfully completed a FW Image write, 
                    // For some components this maybe be the last step of image
                    //   update -> the next line can be commented out.
                    //   For components that employ two banks and do
                    //   ping pong updates - you may have to notify the 
                    //   CFU code that a bank swap is pending. This will ensure
                    //   that the CFU engine will not accept another image
                    //   until the swap occurs. 
                    s_bankSwapPending = TRUE;
                }
                else
                {
                    // Final component specific step of image consumption has failed
                    status = FIRMWARE_UPDATE_STATUS_ERROR_COMPLETE;
                }
            }
        }
        else
        {
            status = FIRMWARE_UPDATE_STATUS_ERROR_WRITE;
        }
    }
    else
    {
        if (ICompFwUpdateBspWrite(pCommand->address, pCommand->pData, pCommand->length, componentId) != 0)
        {
            status = FIRMWARE_UPDATE_STATUS_ERROR_WRITE;
        }
    }

    if (status != FIRMWARE_UPDATE_STATUS_SUCCESS)
    {
        s_currentOffer.updateInProgress = FALSE;
    }

    memset(pResponse, 0, sizeof(FWUPDATE_CONTENT_RESPONSE));
    pResponse->sequenceNumber = sequenceNumber;
    pResponse->status = status;
}

//******************************************************************************
//
// ProcessCFWUOffer - Process the offer component firmware update command.
//
// Input Parameters
//      FWUPDATE_OFFER_COMMAND* pCommand - The command to process.
//      FWUPDATE_OFFER_RESPONSE* pResponse - The response to populate.
//
//******************************************************************************
void ProcessCFWUOffer(FWUPDATE_OFFER_COMMAND* pCommand, 
                      FWUPDATE_OFFER_RESPONSE* pResponse)
{

    // A token is a user-software defined byte.  It's a signature
    // that disambiguates one user-software conducting a CFU from
    // another user-software programming conducting a CFU.  Elect
    // a unique token byte in the user-software design.  The CFU
    // implementation does not define tokens.
    UINT8 token = pCommand->componentInfo.token;

    UINT8 componentId = pCommand->componentInfo.componentId;

    // The last offer isn't completely processed.
    // When the offer that was last offered still remains pending
    // completion, the state is FIRMWARE_UPDATE_OFFER_BUSY.
    // If this condition is detected, return immediately.
    if (s_currentOffer.updateInProgress)
    {
        memset(pResponse, 0, sizeof (FWUPDATE_OFFER_RESPONSE));

        pResponse->status = FIRMWARE_UPDATE_OFFER_BUSY;
        pResponse->rejectReasonCode = FIRMWARE_UPDATE_OFFER_BUSY;
        pResponse->token = token;
        return;
    }

    // Or, if intent is to retrieve the status of a special offer
    // If this condition is detected, return immediately.
    else if (componentId == CFU_SPECIAL_OFFER_CMD)
    {
        FWUPDATE_SPECIAL_OFFER_COMMAND* pSpecialCommand = 
            (FWUPDATE_SPECIAL_OFFER_COMMAND*)pCommand;
        if (pSpecialCommand->componentInfo.commandCode == CFU_SPECIAL_OFFER_GET_STATUS)
        {
            memset(pResponse, 0, sizeof (FWUPDATE_OFFER_RESPONSE));

            pResponse->status = FIRMWARE_UPDATE_OFFER_COMMAND_READY;
            pResponse->token = token;
            return;
        }
    }

    // Or, if the MCU is in progress of swapping Bank 0 for Bank 1 (or vice versa)
    // so the offer is rejected 
    // If this condition is detected, return immediately.
    else if (s_bankSwapPending)
    {
        memset(pResponse, 0, sizeof (FWUPDATE_OFFER_RESPONSE));

        pResponse->status = FIRMWARE_UPDATE_OFFER_REJECT;
        pResponse->rejectReasonCode = FIRMWARE_UPDATE_OFFER_SWAP_PENDING;
        pResponse->token = token;
        return;
    }

    // Else, continue processing the offer by inspecting the registration list.  
    //    Each offer will specify whether or not to:
    //        A) force a MCU reset after processing the offer content
    //        B) ignore the version of the MCU Component Firmware when
    //           analyzing this offers version.
    //
    //    Unless there was overriding flags via "force" or "ignore"
    //    Then the offer is accepted and this notifies the other
    //    user software to move towards the action of submitting Content.
    COMPONENT_REGISTRATION* pRegistration = s_pFirstComponentIFace;

    while (pRegistration)
    {
        if (pRegistration->componentId == componentId)
        {
            BOOL forceReset = pCommand->componentInfo.forceImmediateReset;
            BOOL ignoreVersion = pCommand->componentInfo.forceIgnoreVersion;

            // Found a matching componentId, present the offer to the handler
            pRegistration->interface.ProcessOffer(pCommand, pResponse);

            // If we're ignoring FWVersion check, and that was the reason 
            // the offer was rejected, reverse the decision
            //  NOTE: in released/final Firmware, this ability to accept
            //        any version is usually disabled in FW (this is a TODO
            //        for implementers of CFU
            if (ignoreVersion)
            {
                if ((pResponse->status == FIRMWARE_UPDATE_OFFER_REJECT) 
                        && (pResponse->rejectReasonCode == FIRMWARE_OFFER_REJECT_OLD_FW))
                {
                    pResponse->status = FIRMWARE_UPDATE_OFFER_ACCEPT;
                }
            }

            // This is the point detecting that the offer is accepted, so the logic
            // starts a timer, and copies the flags from the offer.
            if (pResponse->status == FIRMWARE_UPDATE_OFFER_ACCEPT)
            {
                BSP_Timer_Restart(s_updateTimer);
                s_currentOffer.updateInProgress = TRUE;
                s_currentOffer.forceReset = forceReset;
                s_currentOffer.activeComponentId = componentId;
            }

            break;
        }

        pRegistration = pRegistration->pNext;
    }
}

//******************************************************************************
//
// ProcessCFWUGetFWVersion - Process the get fwversion component firmware 
// update command.
//
// Input Parameters
//      GET_FWVERSION_RESPONSE* pResponse - The response to populate.
//
//******************************************************************************
void ProcessCFWUGetFWVersion(GET_FWVERSION_RESPONSE* pResponse)
{
    //Received CFWU GetFwVersion

    ASSERT(pResponse != NULL);

    memset(pResponse, 0, sizeof(GET_FWVERSION_RESPONSE));

    pResponse->header.fwUpdateRevision = CPFWU_REVISION;

    COMPONENT_REGISTRATION* pRegistration = s_pFirstComponentIFace;

    UINT8 componentCount = 0;

    // Fill out the Version and Product Info (variable length)
    pRegistration = s_pFirstComponentIFace;
    UINT32* pVersion = (UINT32*)pResponse->versionAndProductInfoBlob;

    while (pRegistration)
    {
        pRegistration->interface.GetVersion(pVersion);
        *pVersion++;
        pRegistration->interface.GetProductInfo(pVersion);
        *pVersion++;
        pRegistration = pRegistration->pNext;
        componentCount++;
    }

    pResponse->header.componentCount = componentCount;
}

//****************************************************************************
//
// IComponentFirmwareUpdateRegisterComponent - Register component interface
//
// Input Parameters
//      COMPONENT_REGISTRATION* pRegistration.
//
//****************************************************************************
void IComponentFirmwareUpdateRegisterComponent(COMPONENT_REGISTRATION* pRegistration)
{
    if (!pRegistration)
    {
        return;
    }

    // BSP - will need to wrap the below in 
    //       CRITICAL_ENTER();
    {
        pRegistration->pNext = s_pFirstComponentIFace;
        s_pFirstComponentIFace = pRegistration;
    }
    // BSP - CRITICAL_LEAVE();
}

