/**
 * @file coretypes.h
 *
 * @details This file contains the core data types used in WDS firmware.
 *
 */

#ifndef CORETYPES_H
#define CORETYPES_H

//****************************************************************************
//                                  DEFINES
//****************************************************************************
#define NULL            (0)           // NULL pointer
#define NULL_CHAR       ('\0')        // NULL character
#define CARRIAGE_RETURN ('\r')
#define SPACE           (' ')
#define NEWLINE         ('\n')

#define MAX_INT8        ((INT8)   0x7F)
#define MAX_UINT8       ((UINT8)  0xFFu)
#define MAX_INT16       ((INT16)  0x7FFF)
#define MAX_UINT16      ((UINT16) 0xFFFFu)
#define MAX_INT32       ((INT32)  0x7FFFFFFF)
#define MAX_UINT32      ((UINT32) 0xFFFFFFFFu)
#define MAX_INT64       ((INT64)  0x7FFFFFFFFFFFFFFF)
#define MAX_UINT64      ((UINT64) 0xFFFFFFFFFFFFFFFFu)

#define MIN_INT8        ((INT8)   0x80)
#define MIN_UINT8       ((UINT8)  0x0u)
#define MIN_INT16       ((INT16)  0x8000)
#define MIN_UINT16      ((UINT16) 0x0u)
#define MIN_INT32       ((INT32)  0x80000000)
#define MIN_UINT32      ((UINT32) 0x0u)
#define MIN_INT64       ((INT32)  0x8000000000000000)
#define MIN_UINT64      ((UINT32) 0x0u)

#define FALSE           (0)
#define TRUE            (1)

#define SYS_OK          (0)
#define SYS_ERROR       (-1)

//There are some shared files where not all of the api's are used by a given project.
//If not called a warning will be generated.   It is desired to keep the warning for the other
//unintended unused api's. Mark unused API's with the following if desired to keep for other projects.
#define UNUSED_API  __attribute__((unused))

//****************************************************************************
//                                  TYPEDEFS
//****************************************************************************
typedef char *             STRING;
typedef signed char        INT8;      /* 1 byte  */
typedef unsigned char      UINT8;     /* 1 byte  */
typedef signed short       INT16;     /* 2 bytes */
typedef unsigned short     UINT16;    /* 2 bytes */
typedef signed long        INT32;     /* 4 bytes */
typedef unsigned long      UINT32;    /* 4 bytes */
typedef signed long long   INT64;     /* 8 bytes */
typedef unsigned long long UINT64;    /* 8 bytes */
typedef int                BOOL;      /* 4 bytes */

typedef enum
{
    OFF,
    ON,
} OFF_ON;

typedef enum
{
    OPEN,
    CLOSED,
} OPEN_CLOSED;

typedef enum
{
    RELEASED,
    PRESSED,
} RELEASED_PRESSED;

typedef enum
{
    DISABLE,
    ENABLE,
} DISABLE_ENABLE;


typedef void   (*fp_void_void)(void);
typedef UINT32 (*fp_U32_void)(void);
typedef void   (*fp_void_voidPtr)(void* pArg);
typedef UINT16 (*fp_parseDriverRxBuffer)(void* pArg, UINT8* pBuffer, UINT16 bufferLength_bytes, UINT16 readIndex, UINT16 writeIndex);

#endif /* CORETYPES_H */

