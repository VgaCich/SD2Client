/*
*    удалённый доступ
*/

//------------------------------------------------

#include "remacc.h"

//------------------------------------------------

fstring ra_err = "Err\n"; // system error
fstring ra_erc = "ErC\n"; // command error
fstring ra_dat = "Dat\n"; // data following
fstring ra_rdy = "Rdy\n"; // accepted & ready
fstring ra_ecs = "ECS\n"; // checksum error

fstring ra_ti0 = "rx: ";
fstring ra_ti1 = "tx: ";

//------------------------------------------------

dword unhex(char *s)
{
    char c;
    dword val = 0;
    while(c = (*s++))
    {
        if( (c >= '0') && (c <= '9') )
        {
            c -= '0';
        }
        else if( (c >= 'a') && (c <= 'f') )
        {
            c -= 'a' - 10;
        }
        else if( (c >= 'A') && (c <= 'F') )
        {
            c -= 'A' - 10;
        }
        else
        {
            val = 0xFFFFFFFF;
            break;
        }
        val <<= 4;
        val += c;
    }
    return val;
}

//------------------------------------------------

void hex(char *buf, dword val)
{
    byte i;
    char hex[] = "0123456789ABCDEF";
    for(i = 0; i < 8; ++i)
    {
        buf[7 - i] = hex[val & 15];
        val >>= 4;
    }
    buf[8] = 0;
}

//------------------------------------------------

void ra_print_data_size (dword size)
{
	byte k;
	char buf [11], *p;

	if(size < 1000)
	{
		p = buf + 4;
		do {
			*(--p) = (size % 10) + '0';
			size /= 10;
		} while(size);
		buf[4] = ' ';
		buf[5] = 'б';
		buf[6] = 'а';
		buf[7] = 'й';
		buf[8] = 'т';
		buf[9] = 0;
	}
	else
	{
		for(k = 0; size >= 1048576; ++k)
		{
			size >>= 10;
		}
		size = (size * 100) >> 10;
		buf[6] = (size % 10) + '0';
		size /= 10;
		buf[5] = (size % 10) + '0'; 
		size /= 10;
		buf[4] = '.';
		p = buf + 4;
		do {
			*(--p) = (size % 10) + '0';
			size /= 10;
		} while(size);
		buf[7] = ' ';
		switch(k)
		{
		case 0:
			buf[8] = 'К';
			break;
		case 1:
			buf[8] = 'М';
			break;
		case 2:
			buf[8] = 'Г';
			break;
		}
		buf[9] = 'Б';
		buf[10] = 0;
	}

	lcd_puts(p);
}

//------------------------------------------------

void remote_access(void)
{
    dword cmd, val;
    word j;
    byte len1, len2;
    char buf [10];
    dword rx_size;
    dword tx_size;
    
    lcd_clear(0);
    lcd_cputs(ra_ti0);
    lcd_putc('-');
    lcd_clear(1);
    lcd_cputs(ra_ti1);
    lcd_putc('-');
    /*lcd_gotoxy(15,0);
    lcd_putc('\x03');*/
    
    tx_size = 0;
    rx_size = 0;
    
    while(1)
    {
        //подождём входящих данных
        while(!term_chkbuf())
        {
            if(keybd_async)
            {
                keybd_async = 0;
                return;
            }
            //lcd_bat_glyph();
        }
    
        //примем команду и аргумент
        len1 = term_get_string(buf, 9);
        cmd = *((dword*)buf);

        len2 = term_get_string(buf, 9);
        val = unhex(buf);

        if( (len1 != 3) || (len2 == 0) || (val == 0xFFFFFFFF) )
        {
            term_cputs(ra_erc);
        }
        else if(cmd == RA_GET) //считать сектор
        {
            LED_PORT |= LED_GRN;
            if( !sd_prep_read_sector(val) )
            {
                term_cputs(ra_err);
            }
            else
            {
                term_cputs(ra_dat);
                for(j = 0; j < 514; ++j)
                {
                    term_putc(spi_putc(0xFF));
                }
                sd_end_read_sector();

                tx_size += 0x200;
                if( !(tx_size & 0x7ff) )
                {
                    lcd_clear(1);
                    lcd_cputs(ra_ti1);
                    ra_print_data_size(tx_size);
                    bklit_on();
                }

                term_putc('\n');
            }
            LED_PORT &= ~LED_GRN;
        }
        else if(cmd == RA_PUT) //записать сектор
        {
            LED_PORT |= LED_RED;
            if( !sd_prep_write_sector(val) )
            {
                term_cputs(ra_err);
            }
            else
            {
                term_cputs(ra_dat);
                for(j = 0; j < 514; ++j)
                {
                    spi_putc(term_getc());
                }

                rx_size += 0x200;
                if( !(rx_size & 0x7ff) )
                {
                    lcd_clear(0);
                    lcd_cputs(ra_ti0);
                    ra_print_data_size(rx_size);
                    /*lcd_gotoxy(15,0);
                    lcd_putc('\x03');*/
                    bklit_on();
                }

                switch(sd_end_write_sector())
                {
                case 0: term_cputs(ra_err); break;
                case 1: term_cputs(ra_rdy); break;
                case 2: term_cputs(ra_ecs); break;
                }
            }
            LED_PORT &= ~LED_RED;
        }
        else if(cmd == RA_PAR) //количество секторов
        {
            LED_PORT |= LED_BLU;
            if( (val = sd_get_sector_count()) == 0 )
            {
                term_cputs(ra_err);
            }
            else
            {
                hex(buf, val);
                term_cputs(ra_dat);
                term_put_string(buf);
                term_putc('\n');
            }
            LED_PORT &= ~LED_BLU;
        }
        else if(cmd == RA_NES) //количество секторов
        {
            LED_PORT |= LED_BLU;
            if( sd_read((byte*)(&val), (val << 9) + 508, 4) == 0 )
            {
                term_cputs(ra_err);
            }
            else
            {
                hex(buf, val);
                term_cputs(ra_dat);
                term_put_string(buf);
                term_putc('\n');
            }
            LED_PORT &= ~LED_BLU;
        }
        else //неверная команда
        {
            LED_PORT |= LED_BLU;
            term_cputs(ra_erc);
            LED_PORT &= ~LED_BLU;
        }
    }
}

//------------------------------------------------
