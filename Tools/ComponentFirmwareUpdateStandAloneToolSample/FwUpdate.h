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
#define MAKE_STRING_CASE(VAR) case VAR : return (L#VAR)

class FwUpdateCfu
{
public:

    struct CfuHidDeviceConfiguration
    {
        UINT16 Vid;
        UINT16 Pid;
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
                UINT8 Signing : 2;
                UINT8 Type : 2;
                UINT8 Reserved : 3;
                UINT8 Official : 1;
            };
            // 8 bits total
            UINT8 Variant;

        };
        UINT16 Minor;
        UINT8 Major;

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
    } _VersionFormat;

    typedef struct VersionReport
    {
        UINT8 id;
        UINT32 header;
        union
        {
            VersionFormat version;
            UINT32 dVersion;
        };
        UINT32 property;
    } _VersionReport;

    typedef union GenericMessage
    {
        UINT8 id;
        UINT8 dataPayload[60];
    } _GenericMessage;

    typedef struct ComponentPropFormat
    {
        UINT8 Bank : 2;
        UINT8 Rsvd : 2;
        UINT8 Milestone : 4;

        UINT8 ComponentId;
        UINT16 PlatformId;
    } _ComponentPropFormat;


    typedef struct ContentData
    {
        UINT8 id;
        UINT8 flags;
        UINT8 length;
        UINT16 sequenceNumber;
        UINT32 address;
        UINT8 data[MAX_HID_CONTENT_PAYLOAD];
    } _ContentData;

    typedef struct ContentResponseReportBlob
    {
        UINT8 id;
        UINT16 sequenceNumber;
        UINT16 reserved0;

        UINT32 status : 8;
        UINT32 reserved1 : 24;

        UINT32 reserved2;
        UINT32 reserved3;
    } _ContentResponseReportBlob;


    typedef struct ComponentInfo
    {
        // Byte 0
        UINT8 segment;

        // Byte 1
        UINT8 reserved : 6;
        UINT8 forceReset : 1;
        UINT8 forceIgnoreVersion : 1;

        // Byte 2
        UINT8 componentId;

        // Byte 3
        UINT8 token;
    } _ComponentInfo;

    typedef struct ProductInfo
    {
        // Byte 0
        UINT8 protocolRevision : 4;
        UINT8 bank : 2;
        UINT8 reserved0 : 2;

        // Byte 1
        UINT8 milestone : 4;
        UINT8 reserved1 : 4;

        // Byte 2 and 3
        UINT16 platformId;
    } _ProductInfo;


    typedef struct OfferData
    {
        UINT8 id;
        ComponentInfo componentInfo;
        UINT32 version;
        UINT32 compatVariantMask;
        ProductInfo productInfo;
    } _OfferData;

    typedef union OfferDataUnion
    {
        UINT8 data[17];
        OfferData offerData;
    } _OfferDataUnion;

    typedef struct OfferResponseReportBlob
    {
        UINT8 id;
        UINT32 reserved0 : 24;
        UINT32 token : 8;
        UINT32 reserved1;
        UINT32 rrCode : 8;
        UINT32 reserved2 : 24;
        UINT32 status : 8;
        UINT32 reserved3 : 24;

    } _OfferResponseReportBlob;
#pragma pack(pop)

    enum FwUpdateOfferStatus
    {
        // The offer needs to be skipped at this time indicating to 
        // the host to please offer again during next applicable period.
        FIRMWARE_UPDATE_OFFER_SKIP = 0x00,

        // Once FIRMWARE_UPDATE_FLAG_LAST_BLOCK has been issued, 
        // the accessory can then determine if the offer contents 
        // apply to it.  
        FIRMWARE_UPDATE_OFFER_ACCEPT = 0x01,

        // Once FIRMWARE_UPDATE_FLAG_LAST_BLOCK has been issued, 
        // the accessory can then determine if the offer block contents apply to it.  
        FIRMWARE_UPDATE_OFFER_REJECT = 0x02,

        // The offer needs to be delayed at this time.  The device has 
        // nowhere to put the incoming blob.
        FIRMWARE_UPDATE_OFFER_BUSY = 0x03, 

        // Used with the Offer Other response for the OFFER_NOTIFY_ON_READY 
        // request, when the Accessory is ready to accept additional Offers.
        FIRMWARE_UPDATE_OFFER_COMMAND_READY = 0x04, 

        // Response applicable to when the Offer request is not recognized.
        FIRMWARE_UPDATE_CMD_NOT_SUPPORTED = 0xFF
    };

    enum FwUpdateOfferRejectReason
    {
        // The offer was rejected by the product due to the offer 
        // version being older than the currently downloaded / existing firmware.
        FIRMWARE_OFFER_REJECT_OLD_FW = 0x00, //The offer was rejected by the product due to the offer version being older than the currently downloaded / existing firmware.

        // The offer was rejected due to it not being applicable to 
        // the product�s primary MCU(Component ID).
        FIRMWARE_OFFER_REJECT_INV_MCU = 0x01,

        // MCU Firmware has been updated and a swap is currently pending.
        // No further Firmware Update processing can occur until the 
        // target has been reset.
        FIRMWARE_UPDATE_OFFER_SWAP_PENDING = 0x02,

        // The offer was rejected due to a Version mismatch(Debug / Release for example)
        FIRMWARE_OFFER_REJECT_MISMATCH = 0x03,

        // The bank being offered for the component is currently in use.
        FIRMWARE_OFFER_REJECT_BANK = 0x04,

        // The offer's Platform ID does not correlate to the receiving 
        // hardware product.
        FIRMWARE_OFFER_REJECT_PLATFORM = 0x05,

        // The offer's Milestone does not correlate to the receiving 
        // hardware's Build ID.
        FIRMWARE_OFFER_REJECT_MILESTONE = 0x06,

        // The offer indicates an interface Protocol Revision that 
        // the receiving product does not support.
        FIRMWARE_OFFER_REJECT_INV_PCOL_REV = 0x07,

        // The combination of Milestone & Compatibility Variants Mask did 
        // not match the HW.
        FIRMWARE_OFFER_REJECT_VARIANT = 0x08
    };

    inline const wchar_t* OfferStatusToString(UINT32 selection)
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
                // We want the Assert to include the message
                assert(FALSE && L"UNKNOWN_FIRMWARE_UPDATE_OFFER_STATUS");
            }
        }
        return  L"UNKNOWN_FIRMWARE_UPDATE_OFFER_STATUS";
    }

    inline const wchar_t* RejectReasonToString(UINT32 selection)
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
                // We want the assert to include the message
                assert(FALSE &&  L"UNKNOWN_REJECT_REASON");
            }
        }
        return L"UNKNOWN_REJECT_REASON";
    }

    enum FwUpdateCommandFlags
    {
        // Initialize the swap command scratch flash, 
        // erase upper block and copy the factory configuration 
        // parameters to the upper block.  Then either write or 
        // verify the DWord in the command.
        FIRMWARE_UPDATE_FLAG_FIRST_BLOCK = 0x80,

        // Perform CRC, Signature and Version validation after 
        // either writing or verifying the DWORD in the upper block, 
        // per FIRMWARE_UPDATE_FLAG_VERIFY.
        FIRMWARE_UPDATE_FLAG_LAST_BLOCK = 0x40,

        // Verify the byte array in the upper block at the specified address.
        FIRMWARE_UPDATE_FLAG_VERIFY = 0x08,

        FIRMWARE_UPDATE_FLAG_TEST_REPLACE_FILESYSTEM = 0x20
    };

    enum FwUpdateCommandResponseStatus
    {
        // No Error, the requested function(s) succeeded.
        FIRMWARE_UPDATE_SUCCESS = 0x00,

        // Could not either:
        // 1.    Erase the upper block
        // 2.    Initialize the swap command scratch block
        // 3.    Copy the configuration data to the upper block
        FIRMWARE_UPDATE_ERROR_PREPARE = 0x01,

        // Could not write the bytes
        FIRMWARE_UPDATE_ERROR_WRITE = 0x02,

        // Could not set up the swap, in response to 
        // FIRMWARE_UPDATE_FLAG_LAST_BLOCK
        FIRMWARE_UPDATE_ERROR_COMPLETE = 0x03,

        // Verification of the DWord failed, in response to 
        // FIRMWARE_UPDATE_FLAG_VERIFY
        FIRMWARE_UPDATE_ERROR_VERIFY = 0x04,

        // CRC of the image failed, in response to FIRMWARE_UPDATE_FLAG_LAST_BLOCK
        FIRMWARE_UPDATE_ERROR_CRC = 0x05,

        // Firmware signature verification failed, in response to 
        // FIRMWARE_UPDATE_FLAG_LAST_BLOCK
        FIRMWARE_UPDATE_ERROR_SIGNATURE = 0x06,

        // Firmware version verification failed, in response to 
        // FIRMWARE_UPDATE_FLAG_LAST_BLOCK
        FIRMWARE_UPDATE_ERROR_VERSION = 0x07,

        // Firmware has already been updated and a swap is pending.
        // No further Firmware Update commands can be accepted until 
        // the device has been reset.
        FIRMWARE_UPDATE_SWAP_PENDING = 0x08,

        // Firmware has detected an invalid destination address 
        // within the message data content.
        FIRMWARE_UPDATE_ERROR_INVALID_ADDR = 0x09,

        // The Firmware Update Content Command was received without 
        // first receiving a valid & accepted FW Update Offer.
        FIRMWARE_UPDATE_ERROR_NO_OFFER = 0x0A,

        // General error for the Firmware Update Content command, 
        // such as an invalid applicable Data Length.
        FIRMWARE_UPDATE_ERROR_INVALID = 0x0B,
    };

    inline const wchar_t* ContentResponseToString(UINT32 selection)
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
                // We want the assert to include the message
                assert(FALSE && L"UNKNOWN_CONTENT_RESPONSE");
            }
        }
        return L"UNKNOWN_CONTENT_RESPONSE";
                
    }

    typedef struct _PathAndVersion
    {
        std::wstring devicePath;
        VersionReport version;
    }PathAndVersion;

public:

    static FwUpdateCfu* GetInstance()
    {
        static FwUpdateCfu cfu;
        return (FwUpdateCfu*) &cfu;
    }

    _Check_return_
    HRESULT
    RetrieveDevicesWithVersions(_Out_ std::vector<PathAndVersion>& VectorInterfaces,
                                _In_ CfuHidDeviceConfiguration& VrotocolSettings);

    _Check_return_
    HRESULT
    GetVersion(_In_z_ PCWSTR DevicePath, 
               _Out_  VersionReport& VersionReport, 
               _In_   CfuHidDeviceConfiguration& ProtocolSettings);
    
    // Capable of updating device with offer.bin and srec.bin files
    _Check_return_
    BOOL 
    FwUpdateOfferSrec(_In_ CfuHidDeviceConfiguration& ProtocolSettings, 
                      _In_z_ const TCHAR* OfferPath, 
                      _In_z_ const TCHAR* SrecBinPath, 
                      _In_z_ PCWSTR DevicePath, 
                      _In_ UINT8 ForceIgnoreVersion, 
                      _In_ UINT8 ForceReset);

    FwUpdateCfu(const FwUpdateCfu&) = delete;
    void operator=(const FwUpdateCfu&) = delete;

private:

    FwUpdateCfu() noexcept :
        mForceIgnoreVersion(FALSE),
        readEvent(INVALID_HANDLE_VALUE),
        mThreadID(0U) { }

    ~FwUpdateCfu() { }

    BOOL mForceIgnoreVersion;
    UINT32  mThreadID;
    HANDLE readEvent;
};
