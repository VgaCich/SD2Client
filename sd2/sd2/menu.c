//------------------------------------------------

#include "menu.h"

//------------------------------------------------

fchar glyph_prev[8] = "\x02\x06\x0E\x1E\x0E\x06\x02\x00";
fchar glyph_next[8] = "\x08\x0C\x0E\x0F\x0E\x0C\x08\x00";

//------------------------------------------------

void menu_init(void)
{
    lcd_load_cgram(0, glyph_prev);
    lcd_load_cgram(1, glyph_next);
}

//------------------------------------------------

byte menu(fstring title, const __flash fchar* items[], byte k)
{
    byte count;

    for(count = 0; items[count] != 0; ++count)
        ;

    lcd_clear(0);
    lcd_cputs(title);
    
    lcd_gotoxy(15,0);
    lcd_putc(0x03);
    lcd_bat_glyph();

    while(1)
    {
        lcd_clear(1);
        if(k == 0)
        {
            lcd_putc(' ');
        }
        else
        {
            lcd_putc(0);
        }

        lcd_cputs(items[k]);
        
        lcd_gotoxy(15, 1);
        if(k == count-1)
        {
            lcd_putc(' ');
        }
        else
        {
            lcd_putc(1);
        }
        
        switch(keybd_getc())
        {
            case KEY_LEFT:
            case KEY_UP:
                if(k > 0) { k--; }
                break;
                
            case KEY_RIGHT:
            case KEY_DOWN:
                if(k < (count-1)) { k++; }
                break;
                
            case KEY_BACK:
                return 0xFF;
                
            case KEY_ENTER:
                return k;
        }
    }
}

//------------------------------------------------

fstring s_no  = "Нет";
fstring s_yes = "Да";

const __flash fchar* confirm_items[] =
{
    s_no,
    s_yes,
    0
};


//------------------------------------------------

byte confirm(fstring title)
{
    return (menu(title, confirm_items, 0) == 1);
}

//------------------------------------------------

void menu_line(char *s, byte line, byte from, byte to)
{
    byte tlen, wlen;
    byte start, i, k;
    byte timer;
    
    for(tlen = 0; s[tlen]; tlen++)
        ;
    wlen = to - from + 1;
    
    lcd_gotoxy(from, line);
    for(i = 0; (i < tlen) && (i < wlen); ++i)
        lcd_putc(s[i]);

    start = 0;
    timer = 180;
    while(!keybd_async)
    {
        if(tlen > wlen)
        {
            if(!timer)
            {
                lcd_gotoxy(from, line);
                for(i = 0; i < wlen; ++i)
                {
                    k = (start + i) % (tlen+5);
                    if(k >= tlen)
                    {
                        k -= tlen;
                        if( (k >= 1) && (k <= 3) )
                            lcd_putc('*');
                        else
                            lcd_putc(' ');
                    }
                    else
                    {
                        lcd_putc(s[k]);
                    }
                }
    
                timer = 25;
                start++;
            }
            timer--;
                    
            delay_ms(10);
        }
    }
}

//------------------------------------------------
