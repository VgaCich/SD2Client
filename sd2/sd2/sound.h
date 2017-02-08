//------------------------------------------------

#ifndef __sound_h__
#define __sound_h__

//------------------------------------------------

//#include <iom16.h>
//#include <inavr.h>
#include "common.h"
#include "clock.h"
#include "types.h"
#include "keyboard.h"
#include "rsfs.h"
#include "lcd.h"
#include "menu.h"

//------------------------------------------------

#define SOUND_BUF_SIZE  0x40

//------------------------------------------------

#define SND_RATE_0      16000
#define SND_RATE_1      32000

//------------------------------------------------

void sound_init(void);
void player(void);

//------------------------------------------------

#endif /* __sound_h__ */

//------------------------------------------------
