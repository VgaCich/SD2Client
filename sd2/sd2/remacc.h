//------------------------------------------------

#ifndef __remacc_h__
#define __remacc_h__

//------------------------------------------------

//#include <iom16.h>
#include "common.h"
#include "clock.h"
#include "types.h"
#include "sd.h"
#include "lcd.h"
#include "terminal.h"
#include "keyboard.h"
#include "rsfs.h"

//------------------------------------------------

#define RA_MAKECMD(a,b,c,d)     ((dword)(a) | ((dword)(b) << 8) | ((dword)(c) << 16) | ((dword)(d) << 24))

#define RA_GET                  RA_MAKECMD('G','e','t',0)
#define RA_PUT                  RA_MAKECMD('P','u','t',0)
#define RA_PAR                  RA_MAKECMD('P','a','r',0)
#define RA_NES                  RA_MAKECMD('N','e','s',0)

//------------------------------------------------

void remote_access(void);

//------------------------------------------------

#endif /* __remacc_h__ */

//------------------------------------------------
