/*
*   управление LCD 44780
*/

//--------------------------------------------------------------------

#include "lcd.h"

//--------------------------------------------------------------------

tbyte cyr_mapping[64] = 
    "\x41\xA0\x42\xA1\xE0\x45\xA3\xA4"
    "\xA5\xA6\x4B\xA7\x4D\x48\x4F\xA8"
    "\x50\x43\x54\xA9\xAA\x58\xE1\xAB"
    "\xAC\xE2\xAD\xAE\x62\xAF\xB0\xB1"
    "\x61\xB2\xB3\xB4\xE3\x65\xB6\xB7"
    "\xB8\xB9\xBA\xBB\xBC\xBD\x6F\xBE"
    "\x70\x63\xBF\x79\xE4\x78\xE5\xC0"
    "\xC1\xE6\xC2\xC3\xC4\xC5\xC6\xC7";

//--------------------------------------------------------------------

void lcd_nybble(byte nybble, byte rs)
{
    byte i;

    //чистим регистр
    for(i = 0; i < 6; ++i)
    {
        LCDPORT |= LCDSCK;
        delay_250ns();
        LCDPORT &= ~LCDSCK;
    }

    //грузим e
    LCDPORT |= LCDDAT;

    LCDPORT |= LCDSCK;
    delay_250ns();
    LCDPORT &= ~LCDSCK;
 
    //грузим r/s
    if(!rs)
    {
        LCDPORT &= ~LCDDAT;
    }

    delay_250ns();
    LCDPORT |= LCDSCK;
    delay_250ns();
    LCDPORT &= ~LCDSCK;

    //грузим 4 бита данных
    for(i = 0; i < 4; ++i)
    {
        if(nybble & 8)
        {
            LCDPORT |= LCDDAT;
        }
        else
        {
            LCDPORT &= ~LCDDAT;
        }
        nybble <<= 1;

        LCDPORT |= LCDSCK;
        delay_250ns();
        LCDPORT &= ~LCDSCK;
    }

    //посылаем строб
    LCDPORT |= LCDDAT;
    delay_500ns();
    LCDPORT &= ~LCDDAT;
}

//--------------------------------------------------------------------

void lcd_init(void)
{
    //инициализация LCD
    lcd_nybble(LCD_INIT_NYBBLE, 0);
    delay_ms(5);
    lcd_nybble(LCD_INIT_NYBBLE, 0);
    delay_us(160);
    lcd_nybble(LCD_INIT_NYBBLE, 0);
    delay_us(160);
    lcd_nybble(0x2, 0);
    delay_us(160);
    lcd_cmd(LCD_SET_INTERFACE_LENGTH(0, 1, 0));
    lcd_cmd(LCD_ENABLE_DISPLAY_AND_CURSOR(0, 0, 0));
    lcd_cmd(LCD_CLEAR);
    delay_ms(5);
    lcd_cmd(LCD_SET_CURSOR_MOVE_DIRECTION(1, 0));
    lcd_cmd(LCD_ENABLE_DISPLAY_AND_CURSOR(1, 0, 0));
}

//--------------------------------------------------------------------

void lcd_send(byte c, byte rs)
{
    lcd_nybble( ((byte)c) >> 4, rs);
    lcd_nybble( ((byte)c), rs);
    delay_us(80);
}

//--------------------------------------------------------------------

void lcd_cmd(byte cmd)
{
    lcd_send(cmd, 0);
}

//--------------------------------------------------------------------

void lcd_gotoxy(byte x, byte y)
{
    lcd_send(LCD_MOVE_CURSOR_TO_DISPLAY(x + (y << 6)), 0);
}

//--------------------------------------------------------------------

void lcd_putc(char c)
{
    if( (byte)c >= (byte)0xC0 )
    {
        c = cyr_mapping[(byte)(c - 0xC0)];
    }
    else
    {
        switch(c)
        {
        case 'Ё': c = 0xA2; break;
        case 'ё': c = 0xB5; break;
        }
    }
    lcd_send(c, 1);
}

//--------------------------------------------------------------------

void lcd_puts(char *msg)
{
    while(*msg)
    {
        lcd_putc(*msg);
        msg++;
    }
}

//--------------------------------------------------------------------

void lcd_putsn(char *msg, byte n)
{
    while( (*msg) && (n--) )
    {
        lcd_putc(*msg);
        msg++;
    }
}

//--------------------------------------------------------------------

void lcd_cputs(fstring msg)
{
    while(*msg)
    {
        lcd_putc(*msg);
        msg++;
    }
}

//--------------------------------------------------------------------

void lcd_clear(byte ln)
{
    byte i;
    lcd_gotoxy(0, ln);
    for(i = 0; i < 16; ++i)
    {
        lcd_putc(' ');
    }
    lcd_gotoxy(0, ln);
}

//--------------------------------------------------------------------

void lcd_load_cgram(byte pos, fstring pattern)
{
    byte i;
    
    lcd_cmd(LCD_MOVE_CURSOR_TO_CGRAM(pos << 3));
    for(i = 0; i < 8; ++i)
    {
        lcd_send(pattern[i] | 0xE0, 1);
    }
}

//--------------------------------------------------------------------

void lcd_putb(byte n, byte w)
{
    char tmp [4];

    if( ((n > 100) && (w < 3)) || (w > 3) )
    {
        w = 3;
    }
    else if( (n > 10) && (w < 2) )
    {
        w = 2;
    }
    else if( (n > 1) && (w < 1) )
    {
        w = 1;
    }
    
    tmp[w] = 0;

    while(n)
    {
        tmp[--w] = (n % 10) + '0';
        n /= 10;
    }
    
    while(w)
    {
        tmp[--w] = '0';
    }
    
    lcd_puts(tmp);
}

//--------------------------------------------------------------------

#define umax 520 //full charge = 5.20 volt
#define umin 400 //low charge = 4.00 volt
#define bcpos 0x03

void lcd_bat_glyph(void)
{
    byte i;
    word t;
    
    lcd_cmd(LCD_MOVE_CURSOR_TO_CGRAM(bcpos << 3));
    lcd_send(0xEE, 1);
    lcd_send(0xFF, 1);
    t = umax;
    for(i = 0; i < 5; ++i)
    {
        if(volt >= t)
        {
            lcd_send(0xFF, 1);
        }
        else
        {
            lcd_send(0xF1, 1);
        }
        t -= (umax-umin)/5;
    }
    lcd_send(0xFF, 1);
}

//--------------------------------------------------------------------
