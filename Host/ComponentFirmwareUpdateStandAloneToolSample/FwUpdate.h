/*++
    MIT License
    
    Copyright (C) Microsoft Corporation. All rights reserved.

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


Module Name:

    FwUpdate.h

Abstract:
    
    This module implements the Component Firmware Update (CFU) 
    protocol for updating a device's firmware image.

Environment:

    User mode.

--*/

#pragma once

#include <vector>
#include <assert.h>
#define MAX_HID_CONTENT_PAYLOAD 52

class FwUpdateCfu
{
public:

    struct CfuHidDeviceConfiguration
    {
        uint16_t Vid;
        uint16_t Pid;
        USAGE UsagePage;
        USAGE UsageTlc;
        HidReportIdInfo Reports[5];
    };

    enum FwCfuUpdateReports
    {
        FwUpdateVersion = 0,
        FWUpdateOfferResponse,
        FWUpdateContentResponse,
        FWUpdateOffer,
        FWUpdateContent
    };

#pragma pack(push,1)

    enum VariantType
    {
        Debug = 0,
        SelfHost,
        Release,
        Ship
    };

    enum VariantSigning
    {
        Unsigned = 0,
        Test,
        Attestation,
        Production,
    };

    enum VariantOffical
    {
        Private = 0,
        Official
    };

    typedef struct VersionFormat
    {
        union
        {
            struct VariantBreakdown {
                uint8_t Signing : 2;
                uint8_t Type : 2;
                uint8_t Reserved : 3;
                uint8_t Official : 1;
            };
            uint8_t Variant; //8 bits total

        };
        uint16_t Minor;
        uint8_t Major;

        /*
        Major:
        Tracks milestones/branches. Manually updated by product team.
        The normal process when a new release branch is created:
        -New release branch inherits the current version from the 'master' branch.
        -In the 'master' branch, this Major field is updated and Build field is 
        reset to 0 to track the next "in-development" release.

        Example if 'master' version number is 8.15.0, then
        -The new release branch will inherit 8.15.0
        -The new build in the 'master' branch will be updated to 9.0.0

        Minor:
        Indicates one instance of build execution. Automatically updated by the build 
        system or manually reset to 0 when manually increasing the Major field.
        For example in the auto-update scenario, if the prior component version was 
        13.155.* the build system will update it to 13.156.*

        Variant:
        Indicates the variant (aka flavor) of the component build.
        Automatically updated by the build system.

        For example, if the Major and Build fields are 13.155.* the "Debug Unsigned" 
        build version will be 13.155.0, and the "Release AttestationSigned" 
        build version will be 13.155.138
        */
    }_VersionFormat;

    typedef struct VersionReport
    {
        uint8_t id;
        uint32_t header;
        union
        {
            VersionFormat version;
            uint32_t dVersion;
        };
        uint32_t property;
    }_VersionReport;

    typedef union GenericMessage
    {
        uint8_t id;
        uint8_t dataPayload[60];
    }_GenericMessage;

    typedef struct ComponentPropFormat
    {
        uint8_t Bank : 2;
        uint8_t Rsvd : 2;
        uint8_t Milestone : 4;

        uint8_t ComponentId;
        uint16_t PlatformId;
    }_ComponentPropFormat;


    typedef struct ContentData
    {
        uint8_t id;
        uint8_t flags;
        uint8_t length;
        uint16_t sequenceNumber;
        uint32_t address;
        UINT8 data[MAX_HID_CONTENT_PAYLOAD];
    }_ContentData;

    typedef struct ContentResponseReportBlob
    {
        uint8_t id;
        uint16_t sequenceNumber;
        uint16_t reserved0;

        uint32_t status : 8;
        uint32_t reserved1 : 24;

        uint32_t reserved2;
        uint32_t reserved3;
    } _ContentResponseReportBlob;


    typedef struct ComponentInfo
    {
        //Byte 0
        uint8_t segment;

        //Byte 1
        uint8_t reserved : 6;
        uint8_t forceReset : 1;
        uint8_t forceIgnoreVersion : 1;

        //Byte 2
        uint8_t componentId;

        //Byte 3
        uint8_t token;
    } _ComponentInfo;

    typedef struct ProductInfo
    {
        //Byte 0
        uint8_t protocolRevision : 4;
        uint8_t bank : 2;
        uint8_t reserved0 : 2;

        //Byte 1
        uint8_t milestone : 4;
        uint8_t reserved1 : 4;

        //Byte 2 and 3
        uint16_t platformId;
    }_ProductInfo;


    typedef struct OfferData
    {
        uint8_t id;
        ComponentInfo componentInfo;
        uint32_t version;
        uint32_t compatVariantMask;
        ProductInfo productInfo;
    } _OfferData;

    typedef union OfferDataUnion
    {
        uint8_t data[17];
        OfferData offerData;
    } _OfferDataUnion;

    typedef struct OfferResponseReportBlob
    {
        uint8_t id;
        uint32_t reserved0 : 24;
        uint32_t token : 8;

        uint32_t reserved1;

        uint32_t rrCode : 8;
        uint32_t reserved2 : 24;


        uint32_t status : 8;
        uint32_t reserved3 : 24;

    } _OfferResponseReportBlob;

#pragma pack(pop)

    enum FwUpdateOfferStatus
    {
        FIRMWARE_UPDATE_OFFER_SKIP = 0x00, //The offer needs to be skipped at this time, indicating to the host to please offer again during next applicable period.
        FIRMWARE_UPDATE_OFFER_ACCEPT = 0x01, //Once FIRMWARE_UPDATE_FLAG_LAST_BLOCK has been issued, the accessory can then determine if the offer contents apply to it.  If the upda
        FIRMWARE_UPDATE_OFFER_REJECT = 0x02, //Once FIRMWARE_UPDATE_FLAG_LAST_BLOCK has been issued, the accessory can then determine if the offer block contents apply to it.  If th
        FIRMWARE_UPDATE_OFFER_BUSY = 0x03, //The offer needs to be delayed at this time.  The device has nowhere to put the incoming blob.
        FIRMWARE_UPDATE_OFFER_COMMAND_READY = 0x04, //Used with the Offer Other response for the OFFER_NOTIFY_ON_READY request, when the Accessory is ready to accept additional Offers.

        FIRMWARE_UPDATE_CMD_NOT_SUPPORTED = 0xFF //Response applicable to when the Offer request is not recognized.
    };

    enum FwUpdateOfferRejectReason
    {
        FIRMWARE_OFFER_REJECT_OLD_FW = 0x00, //The offer was rejected by the product due to the offer version being older than the currently downloaded / existing firmware.
        FIRMWARE_OFFER_REJECT_INV_MCU = 0x01, //The offer was rejected due to it not being applicable to the product�s primary MCU(Component ID).
        FIRMWARE_UPDATE_OFFER_SWAP_PENDING = 0x02, //MCU Firmware has been updated and a swap is currently pending.No further Firmware Update processing can occur until the blade has been reset.
        FIRMWARE_OFFER_REJECT_MISMATCH = 0x03, //The offer was rejected due to a Version mismatch(Debug / Release for example)
        FIRMWARE_OFFER_REJECT_BANK = 0x04, //The bank being offered for the component is currently in use.
        FIRMWARE_OFFER_REJECT_PLATFORM = 0x05, //The offer�s Platform ID does not correlate to the receiving hardware product.
        FIRMWARE_OFFER_REJECT_MILESTONE = 0x06, //The offer�s Milestone does not correlate to the receiving hardware�s Build ID.
        FIRMWARE_OFFER_REJECT_INV_PCOL_REV = 0x07, //The offer indicates an interface Protocol Revision that the receiving product does not support.
        FIRMWARE_OFFER_REJECT_VARIANT = 0x08  //The combination of Milestone & Compatibility Variants Mask did not match the HW.
    };

#define MAKE_STRING_CASE(VAR) case VAR : return (L#VAR)

    inline const wchar_t* OfferStatusToString(uint32_t selection)
    {
        switch (selection)
        {
            MAKE_STRING_CASE(FIRMWARE_UPDATE_OFFER_SKIP);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_OFFER_ACCEPT);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_OFFER_REJECT);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_OFFER_BUSY);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_OFFER_COMMAND_READY);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_CMD_NOT_SUPPORTED);
            default: 
            {
                assert(FALSE);
                return L"UNKNOWN_FIRMWARE_UPDATE_OFFER_STATUS";
            }
        }
    }

    inline const wchar_t* RejectReasonToString(uint32_t selection)
    {
        switch (selection)
        {
            MAKE_STRING_CASE(FIRMWARE_OFFER_REJECT_OLD_FW);
            MAKE_STRING_CASE(FIRMWARE_OFFER_REJECT_INV_MCU);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_OFFER_SWAP_PENDING);
            MAKE_STRING_CASE(FIRMWARE_OFFER_REJECT_MISMATCH);
            MAKE_STRING_CASE(FIRMWARE_OFFER_REJECT_BANK);
            MAKE_STRING_CASE(FIRMWARE_OFFER_REJECT_PLATFORM);
            MAKE_STRING_CASE(FIRMWARE_OFFER_REJECT_MILESTONE);
            MAKE_STRING_CASE(FIRMWARE_OFFER_REJECT_INV_PCOL_REV);
            MAKE_STRING_CASE(FIRMWARE_OFFER_REJECT_VARIANT);
            default: 
            {
                assert(FALSE);
                return L"UNKNOWN_REJECT_REASON";
            }
        }
    }

    enum FwUpdateCommandFlags
    {
        FIRMWARE_UPDATE_FLAG_FIRST_BLOCK = 0x80,//Initialize the swap command scratch flash, erase upper block and copy the factory configuration parameters to the upper block.  Then either write or verify the DWord in the command.
        FIRMWARE_UPDATE_FLAG_LAST_BLOCK = 0x40, //Perform CRC, Signature and Version validation after either writing or verifying the DWord in the upper block, per FIRMWARE_UPDATE_FLAG_VERIFY.
        FIRMWARE_UPDATE_FLAG_VERIFY = 0x08,     //Verify the byte array in the upper block at the specified address.
        FIRMWARE_UPDATE_FLAG_TEST_REPLACE_FILESYSTEM = 0x20
    };

    enum FwUpdateCommandResponseStatus
    {
        FIRMWARE_UPDATE_SUCCESS = 0x00, //No Error, the requested function(s) succeeded.
        FIRMWARE_UPDATE_ERROR_PREPARE = 0x01, //Could not either:
        //1.    Erase the upper block
        //2.    Initialize the swap command scratch block
        //3.    Copy the configuration data to the upper block
        FIRMWARE_UPDATE_ERROR_WRITE = 0x02, //Could not write the bytes
        FIRMWARE_UPDATE_ERROR_COMPLETE = 0x03, //Could not set up the swap, in response to FIRMWARE_UPDATE_FLAG_LAST_BLOCK
        FIRMWARE_UPDATE_ERROR_VERIFY = 0x04, //Verification of the DWord failed, in response to FIRMWARE_UPDATE_FLAG_VERIFY
        FIRMWARE_UPDATE_ERROR_CRC = 0x05, //CRC of the image failed, in response to FIRMWARE_UPDATE_FLAG_LAST_BLOCK
        FIRMWARE_UPDATE_ERROR_SIGNATURE = 0x06, //Firmware signature verification failed, in response to FIRMWARE_UPDATE_FLAG_LAST_BLOCK
        FIRMWARE_UPDATE_ERROR_VERSION = 0x07, //Firmware version verification failed, in response to FIRMWARE_UPDATE_FLAG_LAST_BLOCK
        FIRMWARE_UPDATE_SWAP_PENDING = 0x08, //Firmware has already been updated and a swap is pending.No further Firmware Update commands can be accepted until the device has been reset.
        FIRMWARE_UPDATE_ERROR_INVALID_ADDR = 0x09, //Firmware has detected an invalid destination address within the message data content.
        FIRMWARE_UPDATE_ERROR_NO_OFFER = 0x0A, //The Firmware Update Content Command was received without first receiving a valid & accepted FW Update Offer.
        FIRMWARE_UPDATE_ERROR_INVALID = 0x0B, //General error for the Firmware Update Content command, such as an invalid applicable Data Length.
    };

    inline const wchar_t* ContentResponseToString(uint32_t selection)
    {
        switch (selection)
        {
            MAKE_STRING_CASE(FIRMWARE_UPDATE_SUCCESS);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_ERROR_PREPARE);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_ERROR_WRITE);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_ERROR_COMPLETE);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_ERROR_VERIFY);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_ERROR_CRC);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_ERROR_SIGNATURE);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_ERROR_VERSION);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_SWAP_PENDING);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_ERROR_INVALID_ADDR);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_ERROR_NO_OFFER);
            MAKE_STRING_CASE(FIRMWARE_UPDATE_ERROR_INVALID);
            default: 
            {
                assert(FALSE);
                return L"UNKNOWN_CONTENT_RESPONSE";
            }
        }
    }

    typedef struct _PathAndVersion
    {
        wstring devicePath;
        VersionReport version;
    }PathAndVersion;

public:
    
    _Check_return_
    static FwUpdateCfu* GetInstance();

    _Check_return_
    HRESULT
    RetrieveDevicesWithVersions(_Out_ vector<PathAndVersion>& VectorInterfaces,
                                _In_ CfuHidDeviceConfiguration& VrotocolSettings);

    _Check_return_
    HRESULT
    GetVersion(_In_z_ PCWSTR DevicePath, 
               _Out_  VersionReport& VersionReport, 
               _In_   CfuHidDeviceConfiguration& ProtocolSettings);
    
    //
    // Capable of updating device with offer.bin and srec.bin files
    // 
    _Check_return_
    bool 
    FwUpdateOfferSrec(_In_ CfuHidDeviceConfiguration& ProtocolSettings, 
                      _In_z_ const TCHAR* OfferPath, 
                      _In_z_ const TCHAR* SrecBinPath, 
                      _In_z_ PCWSTR DevicePath, 
                      _In_ uint8_t ForceIgnoreVersion, 
                      _In_ uint8_t ForceReset);

private:

    FwUpdateCfu() noexcept :
        mForceIgnoreVersion(false),
        readEvent(INVALID_HANDLE_VALUE),
        mThreadID(0) { }
    ~FwUpdateCfu() { ; }

    bool mForceIgnoreVersion;

    static FwUpdateCfu* mpFwUpdateCfu;

    uint32_t  mThreadID;
    HANDLE readEvent;

};