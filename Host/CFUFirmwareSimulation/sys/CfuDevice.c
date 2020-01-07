#include "CfuDevice.h"

#include "CfuDevice.tmh"

VOID
CfuDevice_WriteReport(
    _In_ VOID* VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_ VOID* VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    )
/*++

Routine Description:

    This function is invoked by Vhf when client wants to send a offer/payload related cmd.
    After receiving the UCSI command, this function will -
        - Populate the command buffer and saves in the BufferQueue
        - Trigger the consumer to process the buffer.
        - Acknowledge Vhf, receipt of command.

Arguments:

    VhfClientContext - This Module's handle.
    VhfOperationHandle - Vhf context for this transaction.
    VhfOperationContext - Client context for this transaction.
    HidTransferPacket - Contains Feature report data.

Return Value:

    None

--*/
{
    WDFDEVICE device;
    NTSTATUS ntStatus;
    PDEVICE_CONTEXT deviceContext;
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE currentStatus;
    COMPONENT_FIRMWARE_UPDATE_OFFER_RESPONSE_REJECT_REASON rejectReason;

    VOID* clientBuffer = NULL;
    VOID* clientBufferContext = NULL;

    UNREFERENCED_PARAMETER(VhfOperationContext);

    FuncEntry(TRACE_DEVICE);

    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    device = (WDFDEVICE) VhfClientContext;
    deviceContext = DeviceContextGet(device);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "CfuDevice_WriteReport");

    if (HidTransferPacket->reportBufferLen < (sizeof(OUTPUT_REPORT_LENGTH) + REPORT_ID_LENGTH))
    {
        goto Exit;
    }

    if (HidTransferPacket->reportId != REPORT_ID_PAYLOAD_OUTPUT && HidTransferPacket->reportId != REPORT_ID_OFFER_OUTPUT)
    {
        goto Exit;
    }

    // Get a buffer from BufferPool.
    //
    ntStatus = DMF_BufferQueue_Fetch(deviceContext->DmfModuleResponseBufferQueue,
                                     &clientBuffer,
                                     &clientBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "DMF_BufferQueue_Fetch ntStatus=0x%x", ntStatus);
        goto Exit;
    }

    ASSERT(clientBuffer != NULL);
    ASSERT(clientBufferContext != NULL);
    RESPONSE_BUFFER* responseBuffer = (RESPONSE_BUFFER*)clientBuffer;
    ULONG* responseBufferSize = (ULONG*)clientBufferContext;

    FWUPDATE_OFFER_RESPONSE offerResponse = { 0 };
    FWUPDATE_CONTENT_RESPONSE contentRespose = { 0 };

    switch (HidTransferPacket->reportId)
    {

    case REPORT_ID_OFFER_OUTPUT:
        // NOTE: This could be either of the below.
        // FWUPDATE_OFFER_COMMAND
        // FWUPDATE_OFFER_INFO_ONLY_COMMAND
        // FWUPDATE_OFFER_EXTENDED_COMMAND
        //     Response: FWUPDATE_OFFER_RESPONSE
        FWUPDATE_OFFER_COMMAND* offerCommand = (FWUPDATE_OFFER_COMMAND*)HidTransferPacket->reportBuffer;

        currentStatus = COMPONENT_FIRMWARE_UPDATE_OFFER_ACCEPT;
        rejectReason = 0x0;

        if (offerCommand->ComponentInfo.ComponentId == 0xFF)
        {
            FWUPDATE_OFFER_INFO_ONLY_COMMAND* offerInformation = (FWUPDATE_OFFER_INFO_ONLY_COMMAND*)HidTransferPacket->reportBuffer;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Received Offer Information. Code=0x%x Token = 0x%x", 
                                                               offerInformation->ComponentInfo.InformationCode,
                                                               offerInformation->ComponentInfo.Token);
        }
        else if (offerCommand->ComponentInfo.ComponentId == 0xFE)
        {
            FWUPDATE_OFFER_EXTENDED_COMMAND* offerCommandExt = (FWUPDATE_OFFER_EXTENDED_COMMAND*)HidTransferPacket->reportBuffer;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Received Offer Command. Command=0x%x Token = 0x%x",
                                                               offerCommandExt->ComponentInfo.CommmandCode, 
                                                               offerCommandExt->ComponentInfo.Token);
        }
        else
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Received Offer: \
                                                               Component { Id = 0x%x, V= 0x%x, I = 0x%x, Segment = 0x%x, Token = 0x%x }\
                                                               Version { M = 0x%x, N = 0x%x  variant = 0x%x }",
                                                               offerCommand->ComponentInfo.ComponentId,
                                                               offerCommand->ComponentInfo.ForceIgnoreVersion,
                                                               offerCommand->ComponentInfo.ForceImmediateReset, 
                                                               offerCommand->ComponentInfo.SegmentNumber, 
                                                               offerCommand->ComponentInfo.Token, 
                                                               offerCommand->Version.MajorVersion, 
                                                               offerCommand->Version.MinorVersion, 
                                                               offerCommand->Version.Variant);
            if (!deviceContext->ComponentsUpdated[deviceContext->CurrentComponentIndex])
            {
                if (deviceContext->ComponentIds[deviceContext->CurrentComponentIndex] != offerCommand->ComponentInfo.ComponentId)
                {
                    currentStatus = COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT;
                    rejectReason = COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_INV_MCU;
                }
                else
                {
                    // Save pending version so it can be written when final block is received.
                    // (Just for demonstration purposes.)
                    //
                    deviceContext->PendingComponentVersion[deviceContext->CurrentComponentIndex].MajorVersion = offerCommand->Version.MajorVersion;
                    deviceContext->PendingComponentVersion[deviceContext->CurrentComponentIndex].MinorVersion = offerCommand->Version.MinorVersion;
                    deviceContext->PendingComponentVersion[deviceContext->CurrentComponentIndex].Variant = offerCommand->Version.Variant;
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "ComponenentVersion[%d]=%d:%d:%d [pending]",
                                deviceContext->CurrentComponentIndex,
                                deviceContext->PendingComponentVersion[deviceContext->CurrentComponentIndex].MajorVersion,
                                deviceContext->PendingComponentVersion[deviceContext->CurrentComponentIndex].MinorVersion,
                                deviceContext->PendingComponentVersion[deviceContext->CurrentComponentIndex].Variant);
                }
            }
            else
            {
                currentStatus = COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT;
                rejectReason = COMPONENT_FIRMWARE_UPDATE_OFFER_REJECT_SWAP_PENDING;
            }
        }

        // In either of the case send a success response!
        //
        offerResponse.HidCfuOfferResponse.ReportId = REPORT_ID_OFFER_INPUT;
        offerResponse.HidCfuOfferResponse.CfuOfferResponse.Status = currentStatus;
        offerResponse.HidCfuOfferResponse.CfuOfferResponse.RejectReasonCode = rejectReason;
        offerResponse.HidCfuOfferResponse.CfuOfferResponse.Token = offerCommand->ComponentInfo.Token;
        responseBuffer->ResponseType = OFFER;
        ASSERT(sizeof(responseBuffer->Response) == sizeof(offerResponse.AsBytes));
        RtlCopyMemory(responseBuffer->Response,
                      offerResponse.AsBytes,
                      sizeof(offerResponse.AsBytes));

        break;
    case REPORT_ID_PAYLOAD_OUTPUT:
        // FWUPDATE_CONTENT_COMMAND
        //     Response: FWUPDATE_CONTENT_RESPONSE

        FWUPDATE_CONTENT_COMMAND* contentCommand = (FWUPDATE_CONTENT_COMMAND*)HidTransferPacket->reportBuffer;

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Content Received: \
                                                           { SeqNo = 0x%x Addr = 0x%x, L = 0x%x }",
                                                           contentCommand->SequenceNumber,
                                                           contentCommand->Address, 
                                                           contentCommand->Length);

        if (contentCommand->Flags & COMPONENT_FIRMWARE_UPDATE_FLAG_FIRST_BLOCK)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "First block  Flag set ");
        }

        if (contentCommand->Flags & COMPONENT_FIRMWARE_UPDATE_FLAG_LAST_BLOCK)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Last block Flag set");

            deviceContext->ComponentsUpdated[deviceContext->CurrentComponentIndex] = TRUE;
            deviceContext->ComponentVersion[deviceContext->CurrentComponentIndex].MajorVersion = deviceContext->PendingComponentVersion[deviceContext->CurrentComponentIndex].MajorVersion;
            deviceContext->ComponentVersion[deviceContext->CurrentComponentIndex].MinorVersion = deviceContext->PendingComponentVersion[deviceContext->CurrentComponentIndex].MinorVersion;
            deviceContext->ComponentVersion[deviceContext->CurrentComponentIndex].Variant = deviceContext->PendingComponentVersion[deviceContext->CurrentComponentIndex].Variant;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "ComponenentVersion[%d]=%d:%d:%d [update]",
                        deviceContext->CurrentComponentIndex,
                        deviceContext->ComponentVersion[deviceContext->CurrentComponentIndex].MajorVersion,
                        deviceContext->ComponentVersion[deviceContext->CurrentComponentIndex].MinorVersion,
                        deviceContext->ComponentVersion[deviceContext->CurrentComponentIndex].Variant);

            deviceContext->CurrentComponentIndex = (deviceContext->CurrentComponentIndex + 1) % NUMBER_OF_COMPONENTS;
        }
        if (contentCommand->Flags & COMPONENT_FIRMWARE_UPDATE_FLAG_VERIFY)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Verify Flag set");
        }

        contentRespose.HidCfuContentResponse.ReportId = REPORT_ID_PAYLOAD_INPUT;
        contentRespose.HidCfuContentResponse.CfuContentResponse.Status = COMPONENT_FIRMWARE_UPDATE_SUCCESS;
        contentRespose.HidCfuContentResponse.CfuContentResponse.SequenceNumber = contentCommand->SequenceNumber;
        responseBuffer->ResponseType = CONTENT;
        ASSERT(sizeof(responseBuffer->Response) == sizeof(offerResponse.AsBytes));
        RtlCopyMemory(responseBuffer->Response,
                      contentRespose.AsBytes,
                      sizeof(contentRespose.AsBytes));

        break;
    }

    // Put the command buffer to the consumer.
    //
    *responseBufferSize = sizeof(RESPONSE_BUFFER);
    DMF_BufferQueue_Enqueue(deviceContext->DmfModuleResponseBufferQueue,
                            clientBuffer);
    ntStatus = STATUS_SUCCESS;

Exit:

    // Acknowledge Vhf, receipt of the write.
    //
    DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(deviceContext->DmfModuleVirtualHidDeviceVhf,
                                                          VhfOperationHandle,
                                                          ntStatus);

    if (NT_SUCCESS(ntStatus))
    {
        // Trigger the worker thread to start the work
        //
        DMF_Thread_WorkReady(deviceContext->DmfModuleThread);
    }

    FuncExitVoid(TRACE_DEVICE);
}

VOID
CfuDevice_GetFeatureReport(
    _In_ VOID* VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_ VOID* VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    )
/*++

Routine Description:

    This function is invoked by Vhf when client wants to send a UCSI command to dmf PlatformPolicyManager.
    It return the feature report, and Acknowledges Vhf, receipt of Get.

Arguments:

    VhfClientContext - This Module's handle.
    VhfOperationHandle - Vhf context for this transaction.
    VhfOperationContext - Client context for this transaction.
    HidTransferPacket - Will Contains Feature report data returned.

Return Value:

    None

--*/
{
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS ntStatus;

    FuncEntry(TRACE_DEVICE);

    UNREFERENCED_PARAMETER(VhfOperationContext);
    UNREFERENCED_PARAMETER(HidTransferPacket);

    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    device = (WDFDEVICE)VhfClientContext;
    deviceContext = DeviceContextGet(device);

    if (HidTransferPacket->reportBufferLen < sizeof(GET_FWVERSION_RESPONSE))
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "CfuDevice_GetFeatureReport Size Mismatch 0x%x", HidTransferPacket->reportBufferLen);
        goto Exit;
    }

    if (HidTransferPacket->reportId != REPORT_ID_VERSIONS_FEATURE)
    {
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

    GET_FWVERSION_RESPONSE firmwareVersionResponse = { 0 };
    firmwareVersionResponse.ReportId = HidTransferPacket->reportId;
    firmwareVersionResponse.header.ComponentCount = NUMBER_OF_COMPONENTS;
    firmwareVersionResponse.header.ProtocolRevision = 02;
    firmwareVersionResponse.componentVersionsAndProperty[0].ComponentVersion.AsUInt32 = deviceContext->ComponentVersion[0].AsUInt32;
    firmwareVersionResponse.componentVersionsAndProperty[0].ComponentProperty.ComponentId = deviceContext->ComponentIds[0];
    firmwareVersionResponse.componentVersionsAndProperty[1].ComponentVersion.AsUInt32 = deviceContext->ComponentVersion[1].AsUInt32;
    firmwareVersionResponse.componentVersionsAndProperty[1].ComponentProperty.ComponentId = deviceContext->ComponentIds[1];

    RtlCopyMemory(HidTransferPacket->reportBuffer,
                  &firmwareVersionResponse,
                  sizeof(firmwareVersionResponse));

    HidTransferPacket->reportBufferLen = sizeof(firmwareVersionResponse);
    HidTransferPacket->reportId = REPORT_ID_VERSIONS_FEATURE;

Exit:

    DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(deviceContext->DmfModuleVirtualHidDeviceVhf,
                                                          VhfOperationHandle,
                                                          ntStatus);

    FuncExitVoid(TRACE_DEVICE);
}

NTSTATUS
CfuDevice_ResponseSend(
    _In_ DEVICE_CONTEXT* DeviceContext,
    _In_ RESPONSE_BUFFER* ResponseBuffer
    )
/*++

Routine Description:

    This function completes an input report read.

Arguments:

    DmfModule - This Module's handle.
    ResponseBuffer - Buffer containing the response to be sent.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    switch (ResponseBuffer->ResponseType)
    {
        case OFFER:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Sending Offer Response ReportId=0x%x", ResponseBuffer->Response[0]);
            ntStatus = STATUS_SUCCESS;
            break;
        case CONTENT:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Sending Offer Response ReportId=0x%x", ResponseBuffer->Response[0]);
            ntStatus = STATUS_SUCCESS;
            break;
        default:
            ntStatus = STATUS_UNSUCCESSFUL;
            ASSERT(0);
            break;
    };

    if (NT_SUCCESS(ntStatus))
    {
        HID_XFER_PACKET hidXferPacket;
        RtlZeroMemory(&hidXferPacket, sizeof(hidXferPacket));
        hidXferPacket.reportBuffer = (UCHAR*)&ResponseBuffer->Response;
        hidXferPacket.reportBufferLen = sizeof(ResponseBuffer->Response);
        hidXferPacket.reportId = ResponseBuffer->Response[0];

        // This function actually populates the upper layer's input report.
        //
        ntStatus = DMF_VirtualHidDeviceVhf_ReadReportSend(DeviceContext->DmfModuleVirtualHidDeviceVhf,
                                                          &hidXferPacket);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Send Input Report fails ntStatus=0x%x", ntStatus);
        }
    }

    return ntStatus;
}

#pragma code_seg("PAGED")
NTSTATUS
CfuDevice_EvtDevicePrepareHardware(
    _In_  WDFDEVICE Device,
    _In_  WDFCMRESLIST ResourcesRaw,
    _In_  WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Called when device is added so that resources can be assigned.

Arguments:

    Driver - A handle to a framework driver object.
    ResourcesRaw - A handle to a framework resource-list object that identifies the 
                   raw hardware resources that the Plug and Play manager has assigned to the device.
    ResourcesTranslated - A handle to a framework resource-list object that identifies the 
                   translated hardware resources that the Plug and Play manager has assigned to the device.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    PDEVICE_CONTEXT deviceContext;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    FuncEntry(TRACE_DEVICE);

    ntStatus = STATUS_SUCCESS;
    deviceContext = DeviceContextGet(Device);

    // Set up some default values.
    //
    deviceContext->ComponentIds[0] = COMPONENT_ID_MCU;
    deviceContext->ComponentIds[1] = COMPONENT_ID_AUDIO;
    deviceContext->ComponentVersion[0].MajorVersion = FIRMWARE_VERSION_MAJOR;
    deviceContext->ComponentVersion[1].MajorVersion = FIRMWARE_VERSION_MAJOR;
    deviceContext->ComponentVersion[0].MinorVersion = FIRMWARE_VERSION_MINOR;
    deviceContext->ComponentVersion[1].MinorVersion = FIRMWARE_VERSION_MINOR;
    deviceContext->ComponentVersion[0].Variant = FIRMWARE_VERSION_VARIANT;
    deviceContext->ComponentVersion[1].Variant = FIRMWARE_VERSION_VARIANT;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "ComponenentVersion[%d]=%d:%d:%d [initialize]",
                0,
                deviceContext->ComponentVersion[0].MajorVersion,
                deviceContext->ComponentVersion[0].MinorVersion,
                deviceContext->ComponentVersion[0].Variant);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "ComponenentVersion[%d]=%d:%d:%d [initialize]",
                1,
                deviceContext->ComponentVersion[1].MajorVersion,
                deviceContext->ComponentVersion[1].MinorVersion,
                deviceContext->ComponentVersion[1].Variant);

    // Start the worker thread
    // NOTE: By design, the start/stop of the thread is controlled by the ClientDriver.
    //
    ASSERT(deviceContext->DmfModuleThread != NULL);
    ntStatus = DMF_Thread_Start(deviceContext->DmfModuleThread);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Worker Thread Start fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(TRACE_DEVICE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGED")
NTSTATUS
CfuDevice_EvtDeviceReleaseHardware(
    _In_  WDFDEVICE Device,
    _In_  WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Called when a device is removed.

Arguments:

    Driver - A handle to a framework driver object.
    ResourcesTranslated - A handle to a resource list object that identifies the translated 
                          hardware resources that the Plug and Play manager has assigned to the device.

Return Value:

    NTSTATUS

--*/
{
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS ntStatus = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    FuncEntry(TRACE_DEVICE);

    deviceContext = DeviceContextGet(Device);
    ASSERT(deviceContext != NULL);

    // Make sure to close the worker thread.
    //
    ASSERT(deviceContext->DmfModuleThread != NULL);
    DMF_Thread_Stop(deviceContext->DmfModuleThread);

    return ntStatus;
}
#pragma code_seg()