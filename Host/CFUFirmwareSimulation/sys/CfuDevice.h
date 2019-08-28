#pragma once
#include "Common.h"

#define VENDOR_ID  0x45E
#define PRODUCT_ID 0x111
#define CFU_DEVICE_USAGE_PAGE  0x05, 0xFF
#define CFU_DEVICE_USAGE       0x50

#define REPORT_ID_VERSIONS_FEATURE  0x20
#define REPORT_ID_PAYLOAD_OUTPUT    0x20
#define REPORT_ID_PAYLOAD_INPUT     0x22
#define REPORT_ID_OFFER_OUTPUT      0x25
#define REPORT_ID_OFFER_INPUT       0x25

#define OFFER_INPUT_USAGE_MIN       0x8A
#define OFFER_INPUT_USAGE_MAX       0x8D
#define OFFER_OUTPUT_USAGE_MIN      0x8E
#define OFFER_OUTPUT_USAGE_MAX      0x91
#define PAYLOAD_INPUT_USAGE_MIN     0x66
#define PAYLOAD_INPUT_USAGE_MAX     0x69
#define PAYLOAD_OUTPUT_USAGE        0x61
#define VERSIONS_FEATURE_USAGE      0x62

#define COMPONENT_ID 0x30
#define FIRMWARE_VERSION_MAJOR 123
#define FIRMWARE_VERSION_MINOR 4
#define FIRMWARE_VERSION_VARIANT 5

#define OUTPUT_REPORT_LENGTH 0x3C
#define INPUT_REPORT_LENGTH 0x20

#include <pshpack1.h>
#pragma warning(push)
#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union

typedef union _COMPONENT_VERSION
{
    UINT32 AsUInt32;
    struct
    {
        UINT8 Variant;
        UINT16 MinorVersion;
        UINT8 MajorVersion;
    };
} COMPONENT_VERSION;

typedef union _COMPONENT_PROPERTY
{
    UINT32 AsUInt32;
    struct
    {
        UINT8 Bank : 2;
        UINT8 Reserved: 2;
        UINT8 VendorSpecific0: 4;
        UINT8 ComponentId;
        UINT16 VendorSpecific1;
    };
} COMPONENT_PROPERTY;

typedef struct _COMPONENT_VERSION_AND_PROPERTY
{
    COMPONENT_VERSION ComponentVersion;
    COMPONENT_PROPERTY ComponentProperty;
} COMPONENT_VERSION_AND_PROPERTY;

typedef struct _GET_FWVERSION_RESPONSE
{
    struct
    {
        UINT8 ComponentCount;
        UINT16 Reserved0;
        UINT8 ProtocolRevision : 4;
        UINT8 Reserved1 : 3;
        UINT8 ExtensionFlag : 1;
    } header;
    COMPONENT_VERSION_AND_PROPERTY componentVersionsAndProperty[7];
} GET_FWVERSION_RESPONSE;

typedef struct
{
    struct
    {
        UINT8 SegmentNumber;
        UINT8 Reserved0 : 6;
        UINT8 ForceImmediateReset : 1;
        UINT8 ForceIgnoreVersion : 1;
        UINT8 ComponentId;
        UINT8 Token;
    } ComponentInfo;
    COMPONENT_VERSION Version;
    UINT32 VendorSpecific;
    struct
    {
        UINT8 ProtocolVersion : 4;
        UINT8 Reserved0 : 4;
        UINT8 Reserved1;
        UINT16 VendorSpecific;
    } MiscAndProtocolVersion;
} FWUPDATE_OFFER_COMMAND;

typedef struct
{
    struct
    {
        UINT8 InformationCode;
        UINT8 Reserved0;
        UINT8 ShouldBe0xFF;
        UINT8 Token;
    } ComponentInfo;
    UINT32 Reserved0[3];
} FWUPDATE_OFFER_INFO_ONLY_COMMAND;

typedef struct
{
    struct
    {
        UINT8 CommmandCode;
        UINT8 Reserved0;
        UINT8 ShouldBe0xFE;
        UINT8 Token;
    } ComponentInfo;
    UINT32 Reserved0[3];
} FWUPDATE_OFFER_EXTENDED_COMMAND;

typedef union
{
    UINT16 AsUInt16;
    struct
    {
        UINT8 Reserved0[3];
        UINT8 Token;
        UINT32 Reserved1;
        UINT8 RejectReasonCode;
        UINT8 Reserved2[3];
        UINT8 Status;
        UINT8 Reserved3[3];
    };
} FWUPDATE_OFFER_RESPONSE;

typedef struct _FWUPDATE_CONTENT_COMMAND
{
    UINT8 Flags;
    UINT8 Length;
    UINT16 SequenceNumber;
    UINT32 Address;
    UINT8 Data[52];
} FWUPDATE_CONTENT_COMMAND;

typedef union
{
    UINT16 AsUInt16;
    struct
    {
        UINT16 SequenceNumber;
        UINT16 Reserved0;
        UINT8 Status;
        UINT8 Reserved1[3];
        UINT32 Reserved2[2];
    };
} FWUPDATE_CONTENT_RESPONSE;

typedef enum _RESPONSE_TYPE
{
    OFFER,
    CONTENT
} RESPONSE_TYPE;

typedef struct _RESPONSE_BUFFER
{
    RESPONSE_TYPE ResponseType;
    UINT16 Response;
} RESPONSE_BUFFER;

#pragma warning(pop)

#include <poppack.h>

typedef struct _DEVICE_CONTEXT
{
    // Dmf Module Vhf.
    //
    DMFMODULE DmfModuleVirtualHidDeviceVhf;
    // Version information for this device.
    //
    COMPONENT_VERSION ComponentVersion;
    // Component ID for this device.
    //
    UINT8 ComponentId;
    // Dmf Module response processing Thread.
    //
    DMFMODULE DmfModuleThread;
    // Dmf Module Response BufferQueue.
    //
    DMFMODULE DmfModuleResponseBufferQueue;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceContextGet)


VOID
CfuDevice_WriteReport(
    _In_ VOID* VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_ VOID* VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    );

VOID
CfuDevice_GetFeatureReport(
    _In_ VOID* VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_ VOID* VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    );

NTSTATUS
CfuDevice_ResponseSend(
    _In_ DMFMODULE DmfModule,
    _In_ RESPONSE_BUFFER* ResponseBuffer
    );

NTSTATUS
CfuDevice_EvtDevicePrepareHardware(
    _In_  WDFDEVICE Device,
    _In_  WDFCMRESLIST ResourcesRaw,
    _In_  WDFCMRESLIST ResourcesTranslated
    );

NTSTATUS
CfuDevice_EvtDeviceReleaseHardware(
    _In_  WDFDEVICE Device,
    _In_  WDFCMRESLIST ResourcesTranslated
    );