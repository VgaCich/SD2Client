//--------------------------------------------------------------------

#ifndef __terminal_h__
#define __terminal_h__

//--------------------------------------------------------------------

//#include <iom16.h>
//#include <inavr.h>
#include "common.h"
#include "clock.h"
#include "types.h"

//--------------------------------------------------------------------

#define TERM_BAUD       115200
#define TERM_U2X        1
#define TERM_UBRR       (CLK/((2-TERM_U2X)*8)/TERM_BAUD-1)

#define RXBUF_SIZE      4 // должен быть 2^k

//--------------------------------------------------------------------

void term_init(void);
char term_getc(void);
void term_putc(char c);
void term_cputs(const __flash char s[]);
char term_chkbuf(void);
void term_clrbuf(void);

byte term_get_string(char *buf, byte sz);
void term_put_string(char *s);

//--------------------------------------------------------------------

#endif /* __terminal_h__ */

//--------------------------------------------------------------------
