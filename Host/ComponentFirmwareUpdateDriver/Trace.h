/*++

    Copyright (C) Microsoft. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Trace.h

Abstract:

    Header file for the debug tracing related function defintions and macros.

Environment:

    Windows User-Mode Driver Framework 2.0

--*/

#pragma once

//
// Define the Tracing flags.
//

// NOTE: This Tracing GUID should be updated.
//
// Driver Tracing GUID - {4659F1E1-5645-4657-9D92-E4B8B11D3429}
// DMF Library Tracing GUID - {2AC66594-5157-43B7-82FE-6F462AD84978}
//

#define WPP_CONTROL_GUIDS                                                                                                                                   \
    WPP_DEFINE_CONTROL_GUID(                                                                                                                                \
        ComponentFirmwareUpdateTraceGuid, (4659F1E1,5645,4657,9D92,E4B8B11D3429),                                                                                   \
        WPP_DEFINE_BIT(MYDRIVER_ALL_INFO)                                                                                                                   \
        WPP_DEFINE_BIT(TRACE_DRIVER)                                                                                                                        \
        WPP_DEFINE_BIT(TRACE_DEVICE)                                                                                                                        \
        )                                                                                                                                                   \
    WPP_DEFINE_CONTROL_GUID(                                                                                                                                \
        DMFLibraryTraceGuid, (2AC66594,5157,43B7,82FE,6F462AD84978),                                                                                    \
        WPP_DEFINE_BIT(DMF_TRACE)                                                                                                                       \
        )
//
// This comment block is scanned by the trace preprocessor to define our
// Trace function.
//
// USEPREFIX and USESUFFIX strip all trailing whitespace, so we need to surround
// FuncExit messages with brackets 
//
// begin_wpp config
// FUNC Trace{FLAG=MYDRIVER_ALL_INFO}(LEVEL, MSG, ...);
// FUNC TraceEvents(LEVEL, FLAGS, MSG, ...);
// FUNC FuncEntry{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// FUNC FuncEntryArguments{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS, MSG, ...);
// FUNC FuncExit{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS, MSG, ...);
// FUNC FuncExitVoid{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// FUNC TraceError{LEVEL=TRACE_LEVEL_ERROR}(FLAGS, MSG, ...);
// FUNC TraceInformation{LEVEL=TRACE_LEVEL_INFORMATION}(FLAGS, MSG, ...);
// FUNC TraceVerbose{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS, MSG, ...);
// FUNC FuncExitNoReturn{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// USEPREFIX(FuncEntry, "%!STDPREFIX! [%!FUNC!] --> Entry");
// USEPREFIX(FuncEntryArguments, "%!STDPREFIX! [%!FUNC!] --> Entry");
// USEPREFIX(FuncExit, "%!STDPREFIX! [%!FUNC!] <-- Exit <");
// USESUFFIX(FuncExit, ">");
// USEPREFIX(FuncExitVoid, "%!STDPREFIX! [%!FUNC!] <-- Exit");
// USEPREFIX(TraceError, "%!STDPREFIX! [%!FUNC!] ERROR:");
// USEPREFIX(TraceEvents, "%!STDPREFIX! [%!FUNC!]");
// USEPREFIX(TraceInformation, "%!STDPREFIX! [%!FUNC!]");
// USEPREFIX(TraceVerbose, "%!STDPREFIX! [%!FUNC!]");
// USEPREFIX(FuncExitNoReturn, "%!STDPREFIX! [%!FUNC!] <--");
// end_wpp

// eof: Trace.h
//
