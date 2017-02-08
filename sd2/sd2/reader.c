//------------------------------------------------

#include "reader.h"

//------------------------------------------------

__no_init __eeprom dword ee_tr_bookmarks [NUM_BOOKMARKS][2];

//------------------------------------------------

fstring tr_error = "Ошибка чтения!";
fstring tr_empty = "* Свободно *";
fstring bm_title = "Закладки";

//------------------------------------------------

void text_read(direntry *d, dword *mark)
{
    char sbuf [17];
    char wbuf [16];
    char c;
    
    dword cupo = 0, tebe, t;

    byte wlen = 0;
    byte slen;
    byte sep, psep;

    byte n, i, k;
    byte end;
    
    fs_open(d);
    
    if(mark)
    {
        cupo = *mark;
        fs_seek(cupo, 0);
    }

    lcd_clear(1);
    lcd_clear(0);

    while(1)
    {
        tebe = cupo;
        end = 0;
        for(n = 0; n < 2; ++n)
        {
            slen = 0;
            while(!end)
            {
                while( (!wlen) && (!end) )
                {
                    psep = sep = 0;
                    for(wlen = 0; wlen < 16; ++wlen)
                    {
                        k = fs_read( (byte*)(&c), 1);

                        if(k == 0xFF)
                        {
                            lcd_clear(1);
                            lcd_clear(0);
                            lcd_cputs(tr_error);
                            keybd_getc();
                            return;
                        }
                        if(k == 0)
                        {
                            end = 1;
                            break;
                        }

                        cupo++;
                        
                        if( (c == '\r') || (c == '\n') )
                        {
                            psep = 1;
                            break;
                        }
                        if( (c == '\t') || (c == ' ') )
                        {
                            break;
                        }
                        wbuf[wlen] = c;
                    }
                }
    
                if(slen > 0)
                {
                    if( (wlen > 15 - slen) || (sep) )
                    {
                        break;
                    }
                    sbuf[slen++] = ' ';
                }
    
                for(i = 0; i < wlen; ++i)
                {
                    sbuf[slen++] = wbuf[i];
                }
                wlen = 0;
    
                sep = psep;
            }
    
            sbuf[slen] = 0;
    
            lcd_clear(n);
            lcd_puts(sbuf);
        }

        while(1)
        {
            k = keybd_getc();

            if(k == KEY_LEFT)
            {
                if(tebe < 1024)
                {
                    t = 0;
                }
                else
                {
                    t = tebe - 1024;
                }
                if(fs_seek(t, 0) == 1)
                {
                    cupo = t;
                    wlen = 0;
                    break;
                }
            }

            if(k == KEY_RIGHT)
            {
                t = tebe + 1024;
                if(fs_seek(t, 0) == 1)
                {
                    cupo = t;
                    wlen = 0;
                    break;
                }
            }

            if(k == KEY_UP)
            {
                t = tebe - 32;
                if(fs_seek(t, 0) == 1)
                {
                    cupo = t;
                    wlen = 0;
                    break;
                }
            }

            if(k == KEY_DOWN)
            {
                break;
            }

            if(k == KEY_BACK)
            {
                return;
            }

            if(k == KEY_ENTER)
            {
                if(mark) { *mark = tebe; }
                return;
            }
        }
    }
}

//------------------------------------------------

byte tr_bookmarks(direntry *p, dword *mark, byte open)
{
    byte k = 0, i, used;
    dword ptr, pos, temp;
    direntry d;
    char hex[16] = "0123456789abcdef";

    lcd_clear(0);
    lcd_cputs(bm_title);

    while(1)
    {
        ptr = ee_tr_bookmarks[k][0];
        pos = ee_tr_bookmarks[k][1];
        
        lcd_clear(1);
        if(k != 0)
        {
            lcd_putc(0);
        }
        else
        {
            lcd_putc(' ');
        }
        
        used = 0;
        if( (ptr != 0xFFFFFFFF) &&
            (pos != 0xFFFFFFFF) &&
            (ptr != 0) &&
            (pos != 0) )
        {
            fs_open_entry(ptr);
            if(fs_find(&d, FS_CUR) == 1)
            {
                d.name[8] = 0;
                lcd_puts(d.name);
            
                temp = pos;
                for(i = 0; i < 6; ++i)
                {
                    temp <<= 4;
                    lcd_putc(hex[(temp >> 24) & 0xF]);
                }
                used = 1;
            }
            else
            {
                lcd_cputs(tr_empty);
            }
        }
        else
        {
            lcd_cputs(tr_empty);
        }
        
        lcd_gotoxy(15, 1);
        if(k != NUM_BOOKMARKS - 1)
        {
            lcd_putc(1);
        }
        else
        {
            lcd_putc(' ');
        }
        
        switch(keybd_getc())
        {
            case KEY_LEFT:
            case KEY_UP:
                if(k > 0) { k--; }
                break;
                
            case KEY_RIGHT:
            case KEY_DOWN:
                if(k < NUM_BOOKMARKS-1) { k++; }
                break;
                
            case KEY_ENTER:
                if(open)
                {
                    if(used)
                    {
                        if(fs_find(p, FS_CUR) == 1)
                        {
                            *mark = pos;
                            return 1;
                        }
                    }
                }
                else
                {
                    ee_tr_bookmarks[k][0] = p->entry;
                    ee_tr_bookmarks[k][1] = *mark;
                    return 1;
                }
                break;
                
            case KEY_BACK:
                return 0;
        }
    }

}

//------------------------------------------------

void bm_open(void)
{
    direntry d;
    dword mark, temp;

    if(tr_bookmarks(&d, &mark, 1))
    {
        temp = mark;
        text_read(&d, &mark);
        if(mark != temp)
        {
            tr_bookmarks(&d, &mark, 0);
        }
    }
}

//------------------------------------------------

void tr_open(void)
{
    direntry d;
    dword mark = 0;
    
    if(fs_browse(&d, FS_FILE))
    {
        mark = 0;
        text_read(&d, &mark);
        if(mark != 0)
        {
            tr_bookmarks(&d, &mark, 0);
        }
    }
}

//------------------------------------------------

fstring tr_title = "Читалка";

//fstring tr_item0 = "Закладки";
fstring tr_item1 = "Открыть...";
fstring tr_item2 = "Очистить";
fstring bm_clrng = "Очистка...";

const __flash fchar* tr_menu_items[] =
{
    //tr_item0,
    bm_title,
    tr_item1,
    tr_item2,
    0
};

//------------------------------------------------

void reader(void)
{
    byte k = 0, i;

    while(1)
    {
        switch( (k = menu(tr_title, tr_menu_items, k)) )
        {
        case 0:
            bm_open();
            break;

        case 1:
            tr_open();
            break;
        
        case 2:
            if(confirm(tr_item2))
            {
                lcd_clear(1);
                lcd_cputs(bm_clrng);
                for(i = 0; i < LENGTH(ee_tr_bookmarks); ++i)
                {
                    ee_tr_bookmarks[i][0] = 0ul;
                    ee_tr_bookmarks[i][1] = 0ul;
                }
            }
            break;

        default:
            return;
        }
    }
}

//------------------------------------------------
