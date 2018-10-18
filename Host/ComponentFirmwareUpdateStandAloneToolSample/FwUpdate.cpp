//Copyright(c) Microsoft Corporation.All rights reserved.
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE

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

#define GOTO_FW_UPDATE_EXIT {ret = false; goto Exit;}
FwUpdateCfu* FwUpdateCfu::mpFwUpdateCfu = nullptr;

FwUpdateCfu* FwUpdateCfu::GetInstance()
{
    if (!mpFwUpdateCfu)
    {
        mpFwUpdateCfu = new FwUpdateCfu();
    }

    return mpFwUpdateCfu;
}

bool FwUpdateCfu::RetrieveDevicesWithVersions(
    _Out_ vector<PathAndVersion>& vectorInterfaces,
    _In_ CfuHidDeviceConfiguration& protocolSettings)
{
    GUID deviceInterface;
    CONFIGRET cr;
    PWCHAR pInterfaceList = nullptr;
    PWCHAR pInterface = nullptr;
    vectorInterfaces.clear();

    //Get list of installed HID devices
    HidD_GetHidGuid(&deviceInterface);
    ULONG numCharacters = 0;
    cr = CM_Get_Device_Interface_List_SizeW(
        &numCharacters,
        &deviceInterface,
        nullptr,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS)
    {
        return FALSE;
    }

    //Get a list of the device interfaces
    pInterfaceList = new WCHAR[numCharacters];
    if (!pInterfaceList)
    {
        return FALSE;
    }

    cr = CM_Get_Device_Interface_ListW(
        &deviceInterface,
        nullptr,
        pInterfaceList,
        numCharacters,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS)
    {
        delete[] pInterfaceList;
        return FALSE;
    }

    //Walk and filter the device interface list based on who responds to a version query
    VersionReport version = { 0 };
    pInterface = pInterfaceList;
    while (*pInterface != L'\0')
    {
        if (GetVersion(pInterface, version, protocolSettings))
        {
            PathAndVersion foundDeviceWithVersion = { wstring(pInterface),version };

            wprintf(L"Found device %d:\n", (uint32_t)vectorInterfaces.size());
            wprintf(L"Header 0x%08X\n", version.header);
            wprintf(L"FwVersion %d.%d.%d\n", version.version.Major, version.version.Minor, version.version.Variant);
            wprintf(L"Property 0x%08X\n", version.property);
            wprintf(L"from device %s\n", pInterface);

            vectorInterfaces.push_back(foundDeviceWithVersion);
        }

        pInterface += (wcslen(pInterface) + 1);
    }

    delete[] pInterfaceList;
    return !vectorInterfaces.empty();
}

bool FwUpdateCfu::GetVersion(_In_ PCWSTR DevicePath, _Out_ VersionReport& versionReport, _In_ CfuHidDeviceConfiguration& protocolSettings)
{
    HID_DEVICE device;
    NTSTATUS status;

    //Check that the VID/PID matches.
    wchar_t vidPidFilterString[256];
    memset(&versionReport, 0, sizeof(versionReport));

    if (protocolSettings.Vid && protocolSettings.Pid) //filter on both if both set
    {
        swprintf(vidPidFilterString, 256, L"VID_%04X&PID_%04X", protocolSettings.Vid, protocolSettings.Pid);
        if (!wcsstr(DevicePath, vidPidFilterString))
        {
            //The device found doesn't match the vid and pid
            return false;
        }
    }
    else //filter on vid only (vid is mandatory)
    {
        swprintf(vidPidFilterString, 256, L"VID_%04X", protocolSettings.Vid);
        if (!wcsstr(DevicePath, vidPidFilterString))
        {
            //The device found doesn't match the vid
            return false;
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
        return false;
    }

    //Try to get the device's preparsed HID data.
    if (!HidD_GetPreparsedData(device.hDevice, &device.PreparsedData))
    {
        //wprintf(L"HidD_GetPreparsedData returned false on %s", DevicePath);
        goto exit;
    }

    //Get device capabilities.
    status = HidP_GetCaps(device.PreparsedData, &device.Caps);
    if (!NT_SUCCESS(status))
    {
        wprintf(L"HidP_GetCaps status = %d, %s", status, DevicePath);
        goto exit;
    }

    //Filter out hits for UsagePage
    if (device.Caps.UsagePage != protocolSettings.UsagePage)
    {
        goto exit;
    }

    //Filter out hits for Usage Top Level Collection if set and not matched
    if (protocolSettings.UsageTlc != 0 &&
        device.Caps.Usage != protocolSettings.UsageTlc)
    {
        goto exit;
    }

    if (!HidCommands::PopulateReportId(device, protocolSettings.Reports[FwUpdateVersion]))
    {
        goto exit;
    }

    //Query device for "FeatureVersion" usage. If supported, get the version and return success.

    char reportBuffer[1024];
    uint32_t reportLengthRead = 0;

    if (HidCommands::GetFeatureReport(device, protocolSettings.UsagePage, protocolSettings.Reports[FwUpdateVersion].Usage, reportBuffer, sizeof(reportBuffer), reportLengthRead))
    {
        if (reportLengthRead < sizeof(VersionReport))
        {
            //invalid reportLength read
            wprintf(L"Expected report length of %zu and got %u\n", sizeof(VersionReport), reportLengthRead);
            goto exit;
        }
        versionReport = *reinterpret_cast<VersionReport*>(reportBuffer); //copy to output

        CloseHandle(device.hDevice);
        device.hDevice = INVALID_HANDLE_VALUE;
        return true;
    }

exit:
    //Device didn't support the correct usages.
    CloseHandle(device.hDevice);
    device.hDevice = INVALID_HANDLE_VALUE;
    return false;
}

bool FwUpdateCfu::FwUpdateOfferSrec(
    _In_ CfuHidDeviceConfiguration& protocolSettings,
    _In_ const TCHAR* offerPath, 
    _In_ const TCHAR* srecBinPath,
    _In_ PCWSTR DevicePath,
    _In_ uint8_t forceIgnoreVersion, 
    _In_ uint8_t forceReset)
{
    readEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!readEvent)
    {
        wprintf(L"Failed to create readEvent Handle\n");
        return false;
    }

    HidCommands::READ_THREAD_CONTEXT  readContext;
    HANDLE     ReadThread = 0;
    HID_DEVICE deviceRead;
    HID_DEVICE deviceWrite;
    bool ret = false;
    ifstream offerfilePathStream;
    ifstream filePathStream;

    OfferDataUnion offerDataUnion = { 0 };
    ContentData contentdata = { 0 };

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
        return false;
    }

    //query the device for correct report ids for the configured usages provided in the settings file
    if (!HidCommands::PopulateReportId(deviceRead, protocolSettings.Reports[FWUpdateContent]) ||
        !HidCommands::PopulateReportId(deviceRead, protocolSettings.Reports[FWUpdateContentResponse]) ||
        !HidCommands::PopulateReportId(deviceRead, protocolSettings.Reports[FWUpdateOffer]) ||
        !HidCommands::PopulateReportId(deviceRead, protocolSettings.Reports[FWUpdateOfferResponse])
        )
    {
        wprintf(L"one or more of the 4 update usages were not found on this device\n");
        return false;
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
        return false;
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
        GOTO_FW_UPDATE_EXIT;
    }


    char reportBuffer[REPORT_LENGTH_STANDARD] = { 0 };

    wprintf(L"\n");
    
    //Attempt to open the fw offerPath file
    offerfilePathStream.open(offerPath, ios::binary);
    if (!offerfilePathStream)
    {
        wprintf(L"Error opening offerPath aborting FW Update using %s\n", offerPath);
        GOTO_FW_UPDATE_EXIT;
    }

    char readBuff[16];
    // read data as a block:
    offerfilePathStream.read(readBuff, 16);

    memcpy(&offerDataUnion.data[1], readBuff, sizeof(readBuff));

    //for (size_t i = 0; i < 16; i++)
    //{
    //    printf("%02X", offerDataUnion.data[i + 1]);
    //}
    //printf("\n");
    offerfilePathStream.close();

    //ifstream parser(srecBinPath, ios::binary);
    //Attempt to open the fw srec file
    filePathStream.open(srecBinPath, ios::binary);
    if (!filePathStream)
    {
        wprintf(L"Error opening filepath aborting FW Update: \"%s\"\n", srecBinPath);
        GOTO_FW_UPDATE_EXIT;
    }
    
    HidReportIdInfo report = protocolSettings.Reports[FWUpdateOffer];
    uint32_t reportLength = report.size + 1;

    offerDataUnion.offerData.id = report.id;
    offerDataUnion.offerData.componentInfo.forceReset = forceReset;
    offerDataUnion.offerData.componentInfo.forceIgnoreVersion = forceIgnoreVersion;
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

    report = protocolSettings.Reports[FWUpdateOfferResponse];
    reportBuffer[0] = report.id;
    reportLength = report.size + 1;

    uint32_t waitStatus = WaitForSingleObject(readEvent, READ_THREAD_TIMEOUT_MS);

    //
    // If completionEvent was signaled, then a read just completed
    //   so let's get the status and leave this loop and process the data
    //

    if (WAIT_OBJECT_0 == waitStatus)
    {
        //readEventTriggered

        OfferResponseReportBlob* pOfferResponseReportBlob = reinterpret_cast<OfferResponseReportBlob*>(deviceRead.InputReportBuffer);
        if (pOfferResponseReportBlob->status != FIRMWARE_UPDATE_OFFER_ACCEPT && 
            pOfferResponseReportBlob->status != FIRMWARE_UPDATE_OFFER_COMMAND_READY)
        {
            wprintf(L"FW Update not Accepted for %s\n", offerPath);

            HidCommands::printBuffer(deviceRead.InputReportBuffer, sizeof(OfferResponseReportBlob));

            wprintf(L"status: %s (%d)\n", OfferStatusToString(pOfferResponseReportBlob->status), pOfferResponseReportBlob->status);
            wprintf(L"rrCode: %s (%d)\n", RejectReasonToString(pOfferResponseReportBlob->rrCode), pOfferResponseReportBlob->rrCode);
            wprintf(L"token: %d\n", pOfferResponseReportBlob->token);
            wprintf(L"reserved0: 0x%X\n", pOfferResponseReportBlob->reserved0);
            wprintf(L"reserved1: 0x%X\n", pOfferResponseReportBlob->reserved1);
            wprintf(L"reserved2: 0x%X\n", pOfferResponseReportBlob->reserved2);
            wprintf(L"reserved3: 0x%X\n", pOfferResponseReportBlob->reserved3);

            GOTO_FW_UPDATE_EXIT;
        }
        wprintf(L"FW Update offer accepted for %s\n", offerPath);
    }
    else
    {
        wprintf(L"Timeout while waiting for Offer Command Response Report\n");
        GOTO_FW_UPDATE_EXIT;
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
        
        report = protocolSettings.Reports[FWUpdateContent];
        contentdata.id = report.id;
        reportLength = report.size + 1;

        //subtract startaddress from absolute address
        contentdata.address -= startAddress;

        if (contentPacketsSent+1 == totalContentPacketCount)
        {
            contentdata.flags = FIRMWARE_UPDATE_FLAG_LAST_BLOCK; //Last block
        }

        //Send out the content
        //wprintf(L"\nNow writing sequence %d, %d sector, address 0x%X", contentdata.sequenceNumber, sectorNumber, contentdata.address);
        if (HidCommands::SetOutputReport(deviceWrite, (char*)&contentdata, reportLength))
        {
            //HidCommands::printBuffer((char*)&contentdata, reportLength);
        }
        else
        {
            wprintf(L"error occurred on SetOutputReport 0x%X:\n", contentdata.address);
            GOTO_FW_UPDATE_EXIT;
        }


        bool waiting = true;
        while (waiting)
        {
            uint32_t waitStatus2 = WaitForSingleObject(readEvent, READ_THREAD_TIMEOUT_MS);

            //
            // If completionEvent was signaled, then a read just completed
            //   so let's get the status and leave this loop and process the data 
            //

            if (WAIT_OBJECT_0 == waitStatus2)
            {
                //readEventTriggered   

                ContentResponseReportBlob* pContentResponseReportBlob = reinterpret_cast<ContentResponseReportBlob*>(deviceRead.InputReportBuffer);
                if (pContentResponseReportBlob->status != FIRMWARE_UPDATE_SUCCESS)
                {
                    wprintf(L"\nFW Update not Completed due to content response error\n");

                    HidCommands::printBuffer((char*)&contentdata, reportLength);
                    HidCommands::printBuffer(deviceRead.InputReportBuffer, sizeof(ContentResponseReportBlob));

                    wprintf(L"status: %s (%d)\n", ContentResponseToString(pContentResponseReportBlob->status), pContentResponseReportBlob->status);
                    wprintf(L"sequenceNumber: %d\n", pContentResponseReportBlob->sequenceNumber);

                    GOTO_FW_UPDATE_EXIT;
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
            else  //timeout and therefore waiting longer
            {
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
        ret = TRUE;
    }

    wprintf(L"\n");

    
Exit:
    readContext.TerminateThread = TRUE;
    if(ReadThread) CancelSynchronousIo(ReadThread);

    CloseHandle(deviceRead.hDevice);
    CloseHandle(deviceWrite.hDevice);

    filePathStream.close();
    offerfilePathStream.close();

    deviceRead.hDevice = INVALID_HANDLE_VALUE;
    deviceWrite.hDevice = INVALID_HANDLE_VALUE;

    return ret;
}
