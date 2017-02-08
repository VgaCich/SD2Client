/*
* Serious Device v2.0
* by redsh, 2008-2009
*/

//--------------------------------------------------------------------

#include <iom16.h>
#include <inavr.h>
#include <stdlib.h>
#include "clock.h"
#include "lcd.h"
#include "keyboard.h"
#include "types.h"
#include "sd.h"
#include "terminal.h"
#include "remacc.h"
#include "rsfs.h"
#include "reader.h"
#include "menu.h"
#include "sound.h"
#include "adc.h"

//--------------------------------------------------------------------

__no_init __eeprom byte ee_bklit_cntrl;

//--------------------------------------------------------------------

fstring S_sys_init = "Превед!";
fstring S_sd_error = "SD init error";

//--------------------------------------------------------------------

void bklit_init(void)
{
    byte k;
    
    if( (k = ee_bklit_cntrl) <= 4 )
    {
        bklit_cntrl = k;
    }
}

//--------------------------------------------------------------------

fstring bkl_title = "Подсветка";
fstring bkl_item1 = "Выкл";
fstring bkl_item2 = "25%";
fstring bkl_item3 = "50%";
fstring bkl_item4 = "75%";
fstring bkl_item5 = "100%";

//--------------------------------------------------------------------

const __flash fchar *bkl_menu_items[] =
{
    bkl_item1,
    bkl_item2,
    bkl_item3,
    bkl_item4,
    bkl_item5,
    0
};

//--------------------------------------------------------------------

void backlight_menu(void)
{
    byte k, t;
    
    bklit_cntrl &= 0x7F;
    k = 4 - bklit_cntrl;
    if( (t = menu(bkl_title, bkl_menu_items, k)) != 0xFF )
    {
        if(k != t)
        {
            bklit_cntrl = 4 - t;
            ee_bklit_cntrl = bklit_cntrl;
        }
    }
}

//--------------------------------------------------------------------

void sys_init(void)
{
    DDRA = 0xFF;
    DDRB = 0xB3;
    DDRC = 0xFF;
    DDRD = 0xFE;
    delay_ms(10);
    bklit_init();
    lcd_init();
    menu_init();
    keybd_init();
    term_init();
    spi_init(2);
    if(!sd_init())
    {
        lcd_cputs(S_sd_error);
        exit(0);
        return;
    }
    spi_init(5);
    sound_init();
    adc_init();
    lcd_cputs(S_sys_init);
    __enable_interrupt();
}

//--------------------------------------------------------------------

fstring sd2_title = "SD версия 2.0";
fstring sd2_item1 = "Батарейка";
//fstring sd2_item2 = "Подсветка";
fstring sd2_item3 = "Читалка";
fstring sd2_item4 = "Музыка";
fstring sd2_item5 = "Соединение";

const __flash fchar* sd2_menu_items[] = 
{
    sd2_item1,
    //sd2_item2,
    bkl_title,
    sd2_item3,
    sd2_item4,
    sd2_item5,
    0
};

//--------------------------------------------------------------------

fstring sys_line0 = "Напряж. питания:";
fstring sys_line1 = "Vcc= xx.xx Вольт";

//--------------------------------------------------------------------

void sd2_sys_info(void)
{
    lcd_clear(0);
    lcd_cputs(sys_line0);
    lcd_clear(1);
    lcd_cputs(sys_line1);
    
    while(!keybd_async)
    {
        lcd_gotoxy(5,1);
        lcd_putb(volt/100,2);
        lcd_putc('.');
        lcd_putb(volt%100,2);
        delay_ms(50);
    }
    keybd_async = 0;
}

//--------------------------------------------------------------------

void sd2_main_menu(void)
{
    
    byte k = 0;
    byte t;
    while(1)
    {
        if( (t = menu(sd2_title, sd2_menu_items, k)) != 0xFF )
        {
            switch(t)
            {
            case 0:
                sd2_sys_info();
                break;

            case 1:
                backlight_menu();
                break;

            case 2:
                reader();
                break;
                
            case 3:
                player();
                break;

            case 4:
                remote_access();
                break;
            }
            k = t;
        }
    }
}

//--------------------------------------------------------------------

word adc_getw(void)
{
    ADCSRA |= 0x40;
    while(ADCSRA&0x40)
        ;
    return ADC;
}

//--------------------------------------------------------------------

void main(void)
{
    sys_init();
    adc_enable();
    sd2_main_menu();
    exit(0);
}

//--------------------------------------------------------------------
