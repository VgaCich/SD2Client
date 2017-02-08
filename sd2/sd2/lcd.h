//--------------------------------------------------------------------

#ifndef __lcd_h__
#define __lcd_h__

//--------------------------------------------------------------------

#include <iom16.h>
#include <inavr.h>
#include "clock.h"
#include "types.h"
#include "adc.h"

//--------------------------------------------------------------------

#define LCDPORT     PORTB
#define LCDSCK      1
#define LCDDAT      2

//--------------------------------------------------------------------

#define LCD_CLEAR                                   (0x01)
#define LCD_RETURN_TO_HOME_POSITION                 (0x02)
#define LCD_SET_CURSOR_MOVE_DIRECTION(id,s)         (0x04 | ((id) << 1) | (s))
#define LCD_ENABLE_DISPLAY_AND_CURSOR(d,c,b)        (0x08 | ((d)  << 2) | ((c)  << 1) | (b))
#define LCD_MOVE_CURSOR_AND_SHIFT_DISPLAY(sc,rl)    (0x10 | ((sc) << 3) | ((rl) << 2) )
#define LCD_SET_INTERFACE_LENGTH(dl,n,f)            (0x20 | ((dl) << 4) | ((n)  << 3) | ((f) << 2))
#define LCD_MOVE_CURSOR_TO_CGRAM(addr)              (0x40 | (addr) )
#define LCD_MOVE_CURSOR_TO_DISPLAY(addr)            (0x80 | (addr) )

#define LCD_INIT_NYBBLE                             0x3

//--------------------------------------------------------------------

void lcd_init(void);
void lcd_send(byte c, byte rs);
void lcd_cmd(byte cmd);
void lcd_putc(char c);
void lcd_gotoxy(byte x, byte y);
void lcd_cputs(fstring msg);
void lcd_puts(char *msg);
void lcd_putsn(char *msg, byte n);
void lcd_clear(byte ln);
void lcd_load_cgram(byte pos, fstring pattern);
void lcd_putb(byte n, byte w);
void lcd_bat_glyph(void);

//--------------------------------------------------------------------

#endif /* __lcd_h__ */

//--------------------------------------------------------------------
