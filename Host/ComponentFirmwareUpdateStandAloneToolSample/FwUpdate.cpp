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

    FwUpdate.cpp

Abstract:
    
    This module implements the Component Firmware Update (CFU) 
    protocol for updating a device's firmware image.

Environment:

    User mode.

--*/

#include <string>
#include <iostream>
#include <sstream>
#include <iterator>
#include "tchar.h"
#include <windows.h>
#include <winternl.h>
#include <vector>

#include "HidCommands.h"
#include "FwUpdate.h"
#include "SrecParser.h"

FwUpdateCfu* FwUpdateCfu::mpFwUpdateCfu = nullptr;

_Check_return_
FwUpdateCfu* FwUpdateCfu::GetInstance()
/*++

Routine Description:

    Creates an instance of the CFU singleton object if it does not exits.

Arguments:
    

Return Value:

  Pointer to the CFU object, or NULL on failure.

--*/
{
    if (!mpFwUpdateCfu)
    {
        mpFwUpdateCfu = new (std::nothrow) FwUpdateCfu();
    }

    return mpFwUpdateCfu;
}// GetInstance()


_Check_return_
HRESULT
FwUpdateCfu::RetrieveDevicesWithVersions(_Out_ vector<PathAndVersion>& VectorInterfaces,
                                         _In_ CfuHidDeviceConfiguration& ProtocolSettings)
/*++

Routine Description:

    Attempts to retrieve the device path and version given a specified 
    configuration.

Arguments:
    
    VectorInterfaces -- The list of devices if found.
    
    ProtocolSettings -- The Vendor ID (VID), Product ID (PID), HID usage page
                        and Top Level Collection (TLC) of the device.

Return Value:

  S_OK on success or underlying failure code.

--*/
{
    HRESULT hr = S_OK;
    GUID deviceInterface;
    CONFIGRET cr;
    PWCHAR pInterfaceList = nullptr;
    PWCHAR pInterface = nullptr;
    VectorInterfaces.clear();
    VersionReport version = { 0 };

    //
    // Get list of installed HID devices.
    // 
    HidD_GetHidGuid(&deviceInterface);
    ULONG numCharacters = 0;
    cr = CM_Get_Device_Interface_List_SizeW(
        &numCharacters,
        &deviceInterface,
        nullptr,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS)
    {
        hr = HRESULT_FROM_WIN32(cr);
        goto Exit;
    }

    //
    // Get a list of the device interfaces.
    // 
    pInterfaceList = new (std::nothrow) WCHAR[numCharacters];
    if (!pInterfaceList)
    {
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    cr = CM_Get_Device_Interface_ListW(
        &deviceInterface,
        nullptr,
        pInterfaceList,
        numCharacters,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

    if (cr != CR_SUCCESS)
    {
        hr = HRESULT_FROM_WIN32(cr);
        goto Exit;
    }

    // 
    // Walk and filter the device interface list based on who responds to a 
    // version query.
    // 
    pInterface = pInterfaceList;
    while (*pInterface != L'\0')
    {
        hr = GetVersion(pInterface, version, ProtocolSettings);
        if (SUCCEEDED(hr))
        {
            PathAndVersion foundDeviceWithVersion = { wstring(pInterface),version };

            wprintf(L"Found device %d:\n", (uint32_t)VectorInterfaces.size());
            wprintf(L"Header 0x%08X\n", version.header);
            wprintf(L"FwVersion %d.%d.%d\n", version.version.Major, 
                    version.version.Minor, version.version.Variant);
            wprintf(L"Property 0x%08X\n", version.property);
            wprintf(L"from device %s\n", pInterface);

            VectorInterfaces.push_back(foundDeviceWithVersion);
        }

        pInterface += (wcslen(pInterface) + 1);
    }

    if (VectorInterfaces.size() != 0)
    {
        hr = S_OK;
    }

Exit:

    if (pInterfaceList)
    { 
        delete[] pInterfaceList;
    }
    
    return hr;
}//RetrieveDevicesWithVersions()

_Check_return_
HRESULT
FwUpdateCfu::GetVersion(_In_z_ PCWSTR DevicePath,
                        _Out_  VersionReport& VerReport,
                        _In_   CfuHidDeviceConfiguration& ProtocolSettings)
/*++

Routine Description:

    Given a path and configuration, get the version.

Arguments:
    
    DevicePath       -- Path to the device.
    VerReport        -- The list of devices if found.
    ProtocolSettings -- The Vendor ID (VID), Product ID (PID), HID usage page
                        and Top Level Collection (TLC) of the device.

Return Value:

  S_OK on success or underlying failure code.

--*/

{
    HID_DEVICE device;
    NTSTATUS   status;
    HRESULT    hr = S_OK;
    device.hDevice = INVALID_HANDLE_VALUE;
    
    //Check that the VID/PID matches.
    wchar_t vidPidFilterString[256];
    memset(&VerReport, 0, sizeof(VerReport));

    if (ProtocolSettings.Vid && ProtocolSettings.Pid) //filter on both if both set
    {
        swprintf(vidPidFilterString, 256, L"VID_%04X&PID_%04X", 
                 ProtocolSettings.Vid, ProtocolSettings.Pid);
        if (!wcsstr(DevicePath, vidPidFilterString))
        {
            //The device found doesn't match the vid and pid
            //wprintf(L"The device found does not match the VID/PID\n");
            hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
            goto Exit;

        }
    }
    else //filter on vid only (vid is mandatory)
    {
        swprintf(vidPidFilterString, 256, L"VID_%04X", ProtocolSettings.Vid);
        if (!wcsstr(DevicePath, vidPidFilterString))
        {
            //The device found doesn't match the vid
            hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
            goto Exit;
        }
    }

    //Open a handle to the device.
    device.hDevice = CreateFileW(
        DevicePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (device.hDevice == INVALID_HANDLE_VALUE)
    {
        wprintf(L"INVALID_HANDLE_VALUE %s", DevicePath);
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }

    //Try to get the device's preparsed HID data.
    if (!HidD_GetPreparsedData(device.hDevice, &device.PreparsedData))
    {
        //wprintf(L"HidD_GetPreparsedData returned false on %s", DevicePath);
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }

    //Get device capabilities.
    status = HidP_GetCaps(device.PreparsedData, &device.Caps);
    if (!NT_SUCCESS(status))
    {
        wprintf(L"HidP_GetCaps status = %d, %s", status, DevicePath);
        hr = HRESULT_FROM_WIN32(status);
        goto Exit;
    }

    //Filter out hits for UsagePage
    if (device.Caps.UsagePage != ProtocolSettings.UsagePage)
    {
        hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        goto Exit;
    }

    //Filter out hits for Usage Top Level Collection if set and not matched
    if (ProtocolSettings.UsageTlc != 0 &&
        device.Caps.Usage != ProtocolSettings.UsageTlc)
    {
        hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        goto Exit;
    }

    if (!HidCommands::PopulateReportId(device, ProtocolSettings.Reports[FwUpdateVersion]))
    {
        hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        goto Exit;
    }

    //Query device for "FeatureVersion" usage. If supported, get the version and return success.

    char reportBuffer[1024];
    uint32_t reportLengthRead = 0;

    if (HidCommands::GetFeatureReport(device, ProtocolSettings.UsagePage, 
                                      ProtocolSettings.Reports[FwUpdateVersion].Usage, 
                                      reportBuffer, 
                                      sizeof(reportBuffer), 
                                      reportLengthRead))
    {
        if (reportLengthRead < sizeof(VersionReport))
        {
            //invalid reportLength read
            wprintf(L"Expected report length of %zu and got %u\n", 
                    sizeof(VersionReport), reportLengthRead);
            hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
            goto Exit;
        }

        VerReport = *reinterpret_cast<VersionReport*>(reportBuffer); //copy to output
    }

Exit:
    
    if (device.hDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(device.hDevice);
    }

    return hr;
}//GetVersion()

_Check_return_
bool 
FwUpdateCfu::FwUpdateOfferSrec(
    _In_   CfuHidDeviceConfiguration& ProtocolSettings,
    _In_z_ const TCHAR* OfferPath, 
    _In_z_ const TCHAR* SrecBinPath,
    _In_z_ PCWSTR DevicePath,
    _In_   uint8_t ForceIgnoreVersion, 
    _In_   uint8_t ForceReset)
/*++

Routine Description:

    Attempts to offer a firmware image to the device, and then
    if the offer is accepted deliver the payload.

Arguments:
    
    ProtocolSettings   -- Protocol settings.
    OfferPath          -- Path to the offer input file.
    SrecBinPath        -- Path to the firmware image.
    DevicePath         -- Path to the device to open.
    ForceIgnoreVersion -- Instructs the FW to bypass version checking if allowed/applicable.
    ForceReset         -- Reset the device after fwupdate is complete if supported.

Return Value:

  S_OK on success or underlying failure code.

--*/
{

    HidCommands::READ_THREAD_CONTEXT  readContext;
    HANDLE     ReadThread = 0;
    HID_DEVICE deviceRead;
    HID_DEVICE deviceWrite;
    bool ret = false;
    ifstream offerfilePathStream;
    ifstream filePathStream;
    OfferDataUnion offerDataUnion = { 0 };
    ContentData contentdata = { 0 };

    readEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!readEvent)
    {
        wprintf(L"Failed to create readEvent Handle\n");
        goto Exit;
    }


    deviceRead.hDevice  = INVALID_HANDLE_VALUE;
    deviceWrite.hDevice = INVALID_HANDLE_VALUE;

    deviceRead.hDevice = CreateFileW(
        DevicePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (deviceRead.hDevice == INVALID_HANDLE_VALUE)
    {
        wprintf(L"INVALID_HANDLE_VALUE ");
        wprintf(L"while attempting get handle to %s\n", DevicePath);
        goto Exit;
    }

    //query the device for correct report ids for the configured usages provided in the settings file
    if (!HidCommands::PopulateReportId(deviceRead, ProtocolSettings.Reports[FWUpdateContent]) ||
        !HidCommands::PopulateReportId(deviceRead, ProtocolSettings.Reports[FWUpdateContentResponse]) ||
        !HidCommands::PopulateReportId(deviceRead, ProtocolSettings.Reports[FWUpdateOffer]) ||
        !HidCommands::PopulateReportId(deviceRead, ProtocolSettings.Reports[FWUpdateOfferResponse])
        )
    {
        wprintf(L"one or more of the 4 update usages were not found on this device\n");
        goto Exit;
    }

    deviceWrite.hDevice = CreateFileW(
        DevicePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (deviceWrite.hDevice == INVALID_HANDLE_VALUE)
    {
        wprintf(L"INVALID_HANDLE_VALUE ");
        wprintf(L"while attempting get handle to %s\n", DevicePath);
        goto Exit;
    }

    readContext.readEvent = readEvent;
    readContext.HidDevice = &deviceRead;
    readContext.TerminateThread = FALSE;
    readContext.NumberOfReads = INFINITE_READS;

    ReadThread = CreateThread(
        nullptr,
        0,
        (LPTHREAD_START_ROUTINE)HidCommands::AsynchReadThreadProc,
        (LPVOID)&readContext,
        0,
        (LPDWORD)&mThreadID);
    if (!ReadThread)
    {
        wprintf(L"Failed to create ReadThread\n");
        goto Exit;
    }


    char reportBuffer[REPORT_LENGTH_STANDARD] = { 0 };

    wprintf(L"\n");
    
    //Attempt to open the fw offerPath file
    offerfilePathStream.open(OfferPath, ios::binary);
    if (!offerfilePathStream)
    {
        wprintf(L"Error opening offerPath aborting FW Update using %s\n", OfferPath);
        goto Exit;
    }

    char readBuff[16];
    // read data as a block:
    offerfilePathStream.read(readBuff, 16);

    memcpy(&offerDataUnion.data[1], readBuff, sizeof(readBuff));

    offerfilePathStream.close();

    //Attempt to open the firmware srec file
    filePathStream.open(SrecBinPath, ios::binary);
    if (!filePathStream)
    {
        wprintf(L"Error opening filepath aborting FW Update: \"%s\"\n", SrecBinPath);
        goto Exit;
    }
    
    HidReportIdInfo report = ProtocolSettings.Reports[FWUpdateOffer];
    uint32_t reportLength = report.size + 1;

    offerDataUnion.offerData.id = report.id;
    offerDataUnion.offerData.componentInfo.forceReset = ForceReset;
    offerDataUnion.offerData.componentInfo.forceIgnoreVersion = ForceIgnoreVersion;
    memcpy(reportBuffer, &offerDataUnion, sizeof(offerDataUnion));

    if (HidCommands::SetOutputReport(deviceWrite, reportBuffer, reportLength))
    {
        wprintf(L"SetOutputReport for Offer:\n");
        wprintf(L"bank: %d\n", offerDataUnion.offerData.productInfo.bank);
        wprintf(L"milestone: %d\n", offerDataUnion.offerData.productInfo.milestone);
        wprintf(L"platformId: 0x%x\n", offerDataUnion.offerData.productInfo.platformId);
        wprintf(L"protocolRevision: 0x%x\n", offerDataUnion.offerData.productInfo.protocolRevision);
        wprintf(L"compatVariantMask: 0x%x\n", offerDataUnion.offerData.compatVariantMask);
        wprintf(L"componentId: 0x%x\n", offerDataUnion.offerData.componentInfo.componentId);
        wprintf(L"forceIgnoreVersion: 0x%x\n", offerDataUnion.offerData.componentInfo.forceIgnoreVersion);
        wprintf(L"forceReset: 0x%x\n", offerDataUnion.offerData.componentInfo.forceReset);
        wprintf(L"segment: 0x%x\n", offerDataUnion.offerData.componentInfo.segment);
        wprintf(L"token: 0x%x\n", offerDataUnion.offerData.componentInfo.token);

        //HidCommands::printBuffer(reportBuffer, reportLength);
    }
    else
    {
        HidCommands::printBuffer(reportBuffer, reportLength);
        wprintf(L"SetOutputReport failed with code %d\n", GetLastError());
    }

    report = ProtocolSettings.Reports[FWUpdateOfferResponse];
    reportBuffer[0] = report.id;
    reportLength = report.size + 1;

    uint32_t waitStatus = WaitForSingleObject(readEvent, READ_THREAD_TIMEOUT_MS);

    //
    // If completionEvent was signaled, then a read just completed
    // so let's get the status and leave this loop and process the data
    //

    if (WAIT_OBJECT_0 == waitStatus)
    {
        //readEventTriggered

        OfferResponseReportBlob* pOfferResponseReportBlob = 
            reinterpret_cast<OfferResponseReportBlob*>(deviceRead.InputReportBuffer);
        if (pOfferResponseReportBlob->status != FIRMWARE_UPDATE_OFFER_ACCEPT && 
            pOfferResponseReportBlob->status != FIRMWARE_UPDATE_OFFER_COMMAND_READY)
        {
            wprintf(L"FW Update not Accepted for %s\n", OfferPath);

            HidCommands::printBuffer(deviceRead.InputReportBuffer, sizeof(OfferResponseReportBlob));

            wprintf(L"status: %s (%d)\n", OfferStatusToString(pOfferResponseReportBlob->status), 
                    pOfferResponseReportBlob->status);
            wprintf(L"rrCode: %s (%d)\n", RejectReasonToString(pOfferResponseReportBlob->rrCode), 
                    pOfferResponseReportBlob->rrCode);
            wprintf(L"token: %d\n", pOfferResponseReportBlob->token);
            wprintf(L"reserved0: 0x%X\n", pOfferResponseReportBlob->reserved0);
            wprintf(L"reserved1: 0x%X\n", pOfferResponseReportBlob->reserved1);
            wprintf(L"reserved2: 0x%X\n", pOfferResponseReportBlob->reserved2);
            wprintf(L"reserved3: 0x%X\n", pOfferResponseReportBlob->reserved3);

            goto Exit;
        }
        wprintf(L"FW Update offer accepted for %s\n", OfferPath);
    }
    else
    {
        wprintf(L"Timeout while waiting for Offer Command Response Report\n");
        goto Exit;
    }
    
    contentdata.sequenceNumber = 0;
    contentdata.address = 0;
    contentdata.flags = FIRMWARE_UPDATE_FLAG_FIRST_BLOCK; //first block

    uint32_t startAddress = 0;

    uint32_t totalContentPacketCount = 0;
    uint32_t contentPacketsSent = 0;
    double lastKnownContentCompletionPerc = -1.0;

    //walk the entire file to see how many content packets need to be sent
    while (ProcessSrecBin(filePathStream, contentdata))
    {
        totalContentPacketCount++;
    }

    //Reset stream to start
    filePathStream.clear();
    filePathStream.seekg(0, ios::beg);

    wprintf(L"Beginning content packet transfers:\n");
    while (ProcessSrecBin(filePathStream, contentdata))
    {
        contentdata.flags = 0;

        //establish starting absolute address offset
        if (contentPacketsSent == 0)
        {
            contentdata.flags = FIRMWARE_UPDATE_FLAG_FIRST_BLOCK;
            startAddress = contentdata.address;
        }
        
        report = ProtocolSettings.Reports[FWUpdateContent];
        contentdata.id = report.id;
        reportLength = report.size + 1;

        //subtract the start address from absolute address
        contentdata.address -= startAddress;

        if (contentPacketsSent+1 == totalContentPacketCount)
        {
            contentdata.flags = FIRMWARE_UPDATE_FLAG_LAST_BLOCK; //Last block
        }

        //Send out the content
        if (HidCommands::SetOutputReport(deviceWrite, (char*)&contentdata, reportLength))
        {
            //HidCommands::printBuffer((char*)&contentdata, reportLength);
        }
        else
        {
            wprintf(L"error occurred on SetOutputReport 0x%X:\n", contentdata.address);
            ret = FALSE;
            goto Exit;
        }


        bool waiting = true;
        while (waiting)
        {
            uint32_t waitStatus2 = WaitForSingleObject(readEvent, READ_THREAD_TIMEOUT_MS);

            //
            // If completionEvent was signaled, then a read just completed
            // so let's get the status and leave this loop and process the data 
            //
            if (WAIT_OBJECT_0 == waitStatus2)
            {
                //readEventTriggered   

                ContentResponseReportBlob* pContentResponseReportBlob = 
                    reinterpret_cast<ContentResponseReportBlob*>(deviceRead.InputReportBuffer);
                if (pContentResponseReportBlob->status != FIRMWARE_UPDATE_SUCCESS)
                {
                    wprintf(L"\nFW Update not Completed due to content response error\n");

                    HidCommands::printBuffer((char*)&contentdata, reportLength);
                    HidCommands::printBuffer(deviceRead.InputReportBuffer, sizeof(ContentResponseReportBlob));

                    wprintf(L"status: %s (%d)\n", 
                            ContentResponseToString(pContentResponseReportBlob->status), 
                            pContentResponseReportBlob->status);
                    wprintf(L"sequenceNumber: %d\n", pContentResponseReportBlob->sequenceNumber);

                    goto Exit;
                }
                else if (pContentResponseReportBlob->sequenceNumber != contentdata.sequenceNumber)
                {
                    wprintf(L"\nWaiting for matching ccr to my cr\n");
                }
                else
                {
                    waiting = false;
                }
            }
            else  
            {
                // timeout and therefore waiting longer.
                wprintf(L".");
            }

        }

        contentdata.sequenceNumber++;
        contentPacketsSent++;

        double completionPerc = contentPacketsSent * 100.0 / totalContentPacketCount;
        if (completionPerc >= static_cast<int>(lastKnownContentCompletionPerc + 1)/1)
        {
            wprintf(L"Successfully sent %d content packets (%0.1f%% complete)\n", contentPacketsSent, completionPerc);
            lastKnownContentCompletionPerc = completionPerc;
        }
    }

    //Make we sent 100% of packets
    if ((contentPacketsSent * 100.0 / totalContentPacketCount) < 100.0)
    {
        wprintf(L"Never sent final block command because either srec file not completed or there were no content packets to send in the file\n");
    }
    else
    {
        //
        // All went well.
        //
        ret = true;
    }

    wprintf(L"\n");
    
Exit:
    
    readContext.TerminateThread = TRUE;
    if (ReadThread)
    {
        CancelSynchronousIo(ReadThread);
    }

    if (deviceRead.hDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(deviceRead.hDevice);
    }

    if (deviceWrite.hDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(deviceWrite.hDevice);
    }

    filePathStream.close();
    offerfilePathStream.close();
    
    return ret;
}// FwUpdateOfferSrec()
