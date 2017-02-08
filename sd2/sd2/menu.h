//------------------------------------------------

#ifndef __menu_h__
#define __menu_h__

//------------------------------------------------

//#include <iom16.h>
//#include <inavr.h>
#include "common.h"
#include "clock.h"
#include "types.h"
#include "keyboard.h"
#include "rsfs.h"
#include "lcd.h"

//------------------------------------------------

void menu_init(void);
byte menu(fstring title, const __flash fchar* items[], byte k);
void menu_line(char *s, byte line, byte from, byte to);
byte confirm(fstring title);

//------------------------------------------------

#endif /* __menu_h__ */

//------------------------------------------------
