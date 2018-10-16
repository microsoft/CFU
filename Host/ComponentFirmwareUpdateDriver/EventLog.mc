;/*++
;
;    Copyright (C) Microsoft. All rights reserved.
;    Licensed under the MIT license.
;
;Module Name:
;
;    EventLog.h generated from EventLog.mc using Visual Studio Message Compiler.
;
;Abstract:
;
;    Event log resources.
;
;Environment:
;
;    user mode drivers.
;
;--*/
;

;#pragma once
;

;//
;//  Status values are 32 bit values layed out as follows:
;//
;//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
;//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
;//  +---+-+-------------------------+-------------------------------+
;//  |Sev|C|       Facility          |               Code            |
;//  +---+-+-------------------------+-------------------------------+
;//
;//  where
;//
;//      Sev - is the severity code
;//
;//          00 - Success
;//          01 - Informational
;//          10 - Warning
;//          11 - Error
;//
;//      C - is the Customer code flag
;//
;//      Facility - is the facility code
;//
;//      Code - is the facility's status code
;//
;

MessageIdTypedef=DWORD

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )

FacilityNames=(System=0x0
               Driver=0x1:FACILITY_DRIVER_ERROR_CODE
              )


MessageId=0x0001
Facility=Driver
Severity=Error
SymbolicName=EVENTLOG_MESSAGE_PROTOCOL_START_FAIL
Language=English
Firmware Update Protocol Start Failed: Status: %2.
.

MessageId=0x0002
Facility=Driver
Severity=Warning
SymbolicName=EVENTLOG_MESSAGE_NO_FIRMWARE_INFORMATION
Language=English
Firmware Update Driver: No firmware Information Available!.
.

MessageId=0x0003
Facility=Driver
Severity=Warning
SymbolicName=EVENTLOG_MESSAGE_NO_PROTOCOL_OR_TRANSPORT_INFORMATION
Language=English
Firmware Update Driver: No HID Transport/Protocol Configuration Information Available!.
.
