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

    McuStatus.h

Abstract:

    Common MCU success/error definitions used in component firmware update protocol
    example.

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
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +-----------+---+---------------+-------------------------------+
//  |1 r r r r r|s s|     Module    |            Code               |
//  +-----------+---+---------------+-------------------------------+
//
//  r - Reserved
//  s - Severity - Success, Info, Warning, or Error
//  Module   - MODULE_NUMBER
//  Code     - Unique code for a given module

// MODULE CODES DEFINED HERE
#define MCU_STATUS_MODULE_GENERAL               (0x00UL)
#define MCU_STATUS_MODULE_CFU                   (0x01UL)
// Developer TODO - add your own additional module definitions here if needed

#define SEVERITY_CODE_SUCCESS (0UL)             // MCU_SUCCESS RETURNS TRUE
#define SEVERITY_CODE_INFO    (1UL)             // MCU_SUCCESS RETURNS TRUE
#define SEVERITY_CODE_WARNING (2UL)             // MCU_SUCCESS RETURNS FALSE
#define SEVERITY_CODE_ERROR   (3UL)             // MCU_SUCCESS RETURNS FALSE

#define MCU_STATUS_FLAG       (0x80000000U)

#define MAKE_SEV(s)     ((s & 0x3UL)  << 24)
#define MAKE_MOD(m)     ((m & 0xFFUL) << 16)
#define MAKE_CODE(c)    (c & 0xFFFFUL)
#define MAKE_MCU_STATUS(sev,mod,code) (MCU_STATUS_FLAG | MAKE_SEV(sev) | MAKE_MOD(mod) | MAKE_CODE(code))
// Bit 25 true means warning or error - 0x82000000
#define MCU_STATUS_TEST_MASK                 (MCU_STATUS_FLAG | MAKE_SEV(SEVERITY_CODE_WARNING))

#define GET_SEV_FROM_STATUS(status)          ((UINT32)(((UINT32)(status) >> 24) & 0x3))

#define RETURN(status)                       return status; 
// True for success
#define MCU_SUCCESS(status)                  ((status & MCU_STATUS_TEST_MASK) == MCU_STATUS_FLAG)
// True for fail
#define MCU_FAIL(status)                     ((status & MCU_STATUS_TEST_MASK) != MCU_STATUS_FLAG)
// Bit set to zero for success or selected bit set for a failure
#define MCU_STATUS_BIT(fn, bit)              ((MCU_SUCCESS(fn))? 0 : (1UL << bit))


//****************************************************************************
//
//                                  TYPEDEFS
//
//****************************************************************************
// RETURN CODES DEFINED PER MODULE
typedef enum // MCU_STATUS
{
    // MCU_STATUS_MODULE_GENERAL
    MCU_STATUS_SUCCESS                      = MAKE_MCU_STATUS(SEVERITY_CODE_SUCCESS, MCU_STATUS_MODULE_GENERAL, 0x00UL),
    MCU_STATUS_HANDLED                      = MAKE_MCU_STATUS(SEVERITY_CODE_SUCCESS, MCU_STATUS_MODULE_GENERAL, 0x01UL),
    MCU_STATUS_IN_PROGRESS                  = MAKE_MCU_STATUS(SEVERITY_CODE_SUCCESS, MCU_STATUS_MODULE_GENERAL, 0x02UL),
    MCU_STATUS_COMPLETE                     = MAKE_MCU_STATUS(SEVERITY_CODE_SUCCESS, MCU_STATUS_MODULE_GENERAL, 0x03UL),

    MCU_STATUS_DEFAULT_ERROR                = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_GENERAL, 0x1EUL),
    MCU_STATUS_INVALID_ARG                  = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_GENERAL, 0x1FUL),
    MCU_STATUS_INVALID_STATE                = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_GENERAL, 0x20UL),
    MCU_STATUS_BUSY                         = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_GENERAL, 0x21UL),
    MCU_STATUS_NOT_SUPPORTED                = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_GENERAL, 0x22UL),
    MCU_STATUS_NOT_INITIALIZED              = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_GENERAL, 0x23UL),
    MCU_STATUS_TIMED_OUT                    = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_GENERAL, 0x24UL),

    // MCU_STATUS_MODULE_CFU
	// Supplied below are common flash/NVM error codes.
	// Developer TODO - modify to your implementation specifics
    MCU_STATUS_CFU_FLASH_FAIL               = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x01UL),
    MCU_STATUS_CFU_FLSH_INVALID_SIZE        = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x02UL),
    MCU_STATUS_CFU_FLSH_NULL_VALUE          = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x03UL),
    MCU_STATUS_CFU_FLSH_INVALID_ARGS        = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x04UL),
    MCU_STATUS_CFU_FLSH_ADDR_OUT_OF_BOUNDS  = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x05UL),
    MCU_STATUS_CFU_FLSH_ADDR_NOT_ALIGNED    = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x06UL),
    MCU_STATUS_CFU_CRC_CHECK_NOT_REQUIRED   = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x07UL),
    MCU_STATUS_CFU_ADDR_OUT_OF_BOUNDS       = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x08UL),
    MCU_STATUS_CFU_FLSH_ACCESS_ERROR        = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x09UL),
    MCU_STATUS_CFU_BAD_LOG_CONTEXT          = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x0AUL),
    MCU_STATUS_CFU_NO_PENDING_BLK           = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x0BUL),
    MCU_STATUS_CFU_NOT_OPEN_WRITE           = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x0CUL),
    MCU_STATUS_CFU_SECTION_BUSY             = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x0DUL),
    MCU_STATUS_CFU_SECTION_EMPTY            = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x0EUL),
    MCU_STATUS_CFU_SECTION_FULL             = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x0FUL),
    MCU_STATUS_CFU_SECTION_DISABLED         = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x10UL),
    MCU_STATUS_CFU_SECTION_DIRTY            = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x11UL),
    MCU_STATUS_CFU_BAD_SECTION              = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x12UL),
    MCU_STATUS_CFU_NO_MORE_SECTIONS         = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x13UL),
    MCU_STATUS_CFU_CRC_CHECK_FAIL           = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x14UL),
    MCU_STATUS_CFU_READ_ERROR               = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x15UL),
    MCU_STATUS_CFU_WRITE_ERROR              = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x16UL),
    MCU_STATUS_CFU_ERASE_ERROR              = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x17UL),
    MCU_STATUS_CFU_BAD_BLOCK_HEADER         = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x18UL),
    MCU_STATUS_CFU_NO_PENDING_BLOCK         = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x19UL),
    MCU_STATUS_CFU_NOT_OPEN_FOR_WRITE       = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x1AUL),
    MCU_STATUS_CFU_BAD_BLOCK_INDEX          = MAKE_MCU_STATUS(SEVERITY_CODE_ERROR,   MCU_STATUS_MODULE_CFU, 0x1BUL),
	
} MCU_STATUS;

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

