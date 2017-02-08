//--------------------------------------------------------------------

#ifndef __types_h__
#define __types_h__

//--------------------------------------------------------------------

#include "common.h"

//--------------------------------------------------------------------

typedef unsigned char byte, uchar;
typedef unsigned short word, ushort;
typedef unsigned long dword, ulong;

//--------------------------------------------------------------------

typedef const __flash char fstring[];
typedef const __flash char fchar;
typedef const __flash byte fbyte;

//--------------------------------------------------------------------

typedef const __tinyflash char tstring[];
typedef const __tinyflash char tchar;
typedef const __tinyflash byte tbyte;

//--------------------------------------------------------------------

#define LENGTH(a)   (sizeof(a)/sizeof(a[0]))

//--------------------------------------------------------------------

#endif /* __types_h__ */

//--------------------------------------------------------------------
