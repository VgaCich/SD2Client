//------------------------------------------------

#include "rsfs.h"

//------------------------------------------------

fstring fs_empty = "* Нет файлов *";
fstring fs_error = "* Ошибка! *";

//------------------------------------------------

dword fs_base_ptr = 0;
dword fs_read_ptr = 0;
dword fs_file_ptr = 0;
dword fs_file_siz = 0;

//------------------------------------------------

void fs_open(direntry *p)
{
    fs_base_ptr = p->start;
    fs_read_ptr = p->start;
    fs_file_siz = p->size;
    fs_file_ptr = 0;
}

//------------------------------------------------

void fs_open_entry(dword ptr)
{
    fs_base_ptr = ptr;
    fs_read_ptr = ptr;
    fs_file_siz = 0xFFFFFFFF;
    fs_file_ptr = 0;
}

//------------------------------------------------

/*
*   fs_skip
*
*   все байты пропущены -> 1
*   серьёзная ошибка -> 0xFF
*   нельзя пропустить (конец файла) -> 0
*
*   использует:
*       fs_base_ptr
*       fs_read_ptr
*
*   изменяет (только в случае успеха):
*       fs_read_ptr
*/

byte fs_skip(dword count)
{
    dword cuse;
    dword temp;
    
    //для оптимзации
    if(count == 0)
    {
        return 1;
    }
    
    //адрес начала "текущего" сектора
    cuse = fs_read_ptr & ~0x1FF;
    
    //количество байт от начала "текущего" сектора
    count += fs_read_ptr & 0x1FF;

    //нужно пропустить сектор (или нексколько секторов)
    while(count >= 508)
    {
        //отмечаем, что пропустили сектор
        count -= 508;

        //адрес конца тек. сектора / адреса след. сектора
        temp = cuse + 508;

        //читаем индекс след. сектора
        if(!sd_read( (byte*)(&cuse), temp, 4))
        {
            return 0xFF;
        }

        //след. сектора нету (конец файла)
        if(cuse == 0xFFFFFFFF)
        {
            //все байты "пропустили"
            if(!count)
            {
                fs_read_ptr = temp;
                return 1;
            }
            
            //остались "непропущеные" байты
            return 0;
        }
        
        //индекс -> адрес
        cuse <<= 9;
    }

    //адрес нового сектора + "оставшиеся" байты
    fs_read_ptr = cuse + count;

    return 1;
}

//------------------------------------------------

/*
*   fs_seek
*
*   переход на нужный адрес -> 1
*   нельзя перейти (вылезаем за пределы файла) -> 0
*   серьёзная ошибка -> 0xFF
*
*   использует:
*       fs_base_ptr
*       fs_read_ptr
*       fs_file_siz
*       fs_file_ptr
*
*   изменяет (только в случае успеха):
*       fs_read_ptr
*       fs_file_ptr
*/

byte fs_seek(dword offset, byte curor)
{
    byte k;
    
    //считаем адрес, куда надо перейти
    if(curor)
    {
        offset += fs_file_ptr;
    }

    //вылезли за пределы файла (вперёд или назад)
    if( (offset > 0x7FFFFFFF) || 
        (offset > fs_file_siz) )
    {
        return 0;
    }
    
    //нужно перейти "вперёд" (проверяем для оптимизации)
    if(offset > fs_file_ptr)
    {
        if( (k = fs_skip(offset - fs_file_ptr)) == 1 )
        {
            fs_file_ptr = offset;
        }
        return k;
    }

    //нужно вернуться "назад"
    fs_read_ptr = fs_base_ptr;
    if( (k = fs_skip(offset)) == 1 )
    {
        fs_file_ptr = offset;
    }
    return k;
}

//------------------------------------------------

/*
*   fs_read
*
*   в случае успеха -> кол-во считаных байт
*   серьёзная ошибка -> 0xFF
*
*   использует:
*       fs_read_ptr
*       fs_file_siz
*       fs_file_ptr
*
*
*   изменяет (только в случае успеха):
*       fs_read_ptr
*       fs_file_ptr
*/

byte fs_read(byte *buf, byte count)
{
    dword filetail; //"хвост" файла
    dword temp;     //всякое такое
    dword cupo;     //откуда читаем
    word tail;      //"хвост" сектора
    byte cb;        //сколько будем читать

    //считаем "хвост" файла
    filetail = fs_file_siz - fs_file_ptr;

    //читать будем не больше "хвоста"
    if( (dword)count > filetail )
    {
        count = (byte)filetail;
    }

    //небольшая оптимизация
    if(count == 0)
    {
        return 0;
    }

    //сколько нужно прочитать
    cb = count;

    //откуда будем читать
    cupo = fs_read_ptr;

    //считаем "хвост" сектора
    tail = (word)(508 - (cupo & 0x1FF)); //sector's tail

    //нужно дочитать сектор и "начать" следующий
    if(count >= (word)tail)
    {
        //читаем хвост сектора
        if(sd_read(buf, cupo, (byte)tail) == 0)
        {
            return 0xFF;
        }

        //адрес индекса след. сектора / конец тек.сектора
        temp = (cupo & ~0x1FF) + 508;
        if( sd_read( (byte*)(&cupo), temp, 4) == 0 )
        {
            return 0xFF;
        }

        //след. сектора нет (файл закончился)?
        if(cupo == 0xFFFFFFFF)
        {
            fs_read_ptr = temp;
            fs_file_ptr += (dword)tail;
            return (byte)tail;
        }

        //индекс -> адрес
        cupo <<= 9;

        //отмечаем прочитаные байты
        count -= (byte)tail;
        buf += (byte)tail;
    }

    //небольшая оптимизация
    if(count > 0)
    {
        //читаем текущий сектор
        if( sd_read(buf, cupo, count) == 0 )
        {
            return 0xFF;
        }
    
        cupo += (dword)count;
    }

    fs_read_ptr = cupo;
    fs_file_ptr += (dword)cb;
    return cb;
}

//------------------------------------------------

/*
*   fs_find
*
*   успех -> 1
*   неудача -> 0
*   серьёзная ошибка -> 0xFF
*/

byte fs_find(direntry *p, byte mode)
{
    dword temp;
    byte k;

    if(mode == FS_ROOT)
    {
        //перейти к корневой диртории
        if( (temp = sd_get_sector_count()) == 0)
        {
            return 0xFF;
        }
        temp = ((temp + 4095) >> 12) << 9;
    
        fs_base_ptr = temp;
        fs_read_ptr = temp;
        fs_file_ptr = 0;
        return 1;
    }
    
    if(mode == FS_FIRST)
    {
        fs_read_ptr = fs_base_ptr;
        fs_file_ptr = 0;
    }
    else if(mode == FS_NEXT)
    {
        if( (k = fs_seek(254, 1)) != 1)
        {
            return k;
        }
    }

    while(1)
    {
        //идём назад?
        if(mode == FS_PREV)
        {
            if( (k = fs_seek((dword)(-254), 1)) != 1)
            {
                return k;
            }
        }
        
        //читаем элемент каталога
        p->entry = fs_read_ptr;

        p->name[32] = 0;
        if( (k = fs_read( (byte*)(p->name), 32)) != 32)
        {
            if(k == 0)
            {
                return 0;
            }
            return 0xFF;
        }
        if(fs_seek(58, 1) != 1)
        {
            return 0xFF;
        }
        if(fs_read( (byte*)(&(p->start)), 9) != 9)
        {
            return 0xFF;
        }
        if(fs_seek((dword)(-99), 1) != 1)
        {
            return 0xFF;
        }
        
        //нашли?
        if(p->isdir != 0xFF)
        {
            break;
        }
        
        //идём вперёд?
        if(mode != FS_PREV)
        {
            if( (k = fs_seek(254, 1)) != 1)
            {
                return k;
            }
        }
    }

    p->start <<= 9;
    p->isdir &= 0x10;

    return 1;
}

//------------------------------------------------

/*
*   fs_browse
*
*   успех -> 1
*   неудача -> 0
*   серьёзная ошибка -> 0xFF
*/

byte fs_browse(direntry *p, byte mode)
{
    byte k;
    byte h = 0;
    byte b, o;

    dword paen [12];
    dword cuen [12];

    dword temp1, temp2;
    
    paen[0] = 0;
    cuen[0] = 0;

    //p->name[16] = 0;
    
    fs_file_siz = 0xFFFFFFFF;

    while(1)
    {
        lcd_clear(0);

        if(h == 0)
        {
            //перейти к корневому каталогу
            if(fs_find(p, FS_ROOT) == 0xFF)
            {
                lcd_cputs(fs_error);
                keybd_getc();
                return 0xFF;
            }

            //пишем имя каталога
            lcd_putc('/');
        }
        else
        {
            //перейти к каталогу paen[h]
            fs_base_ptr = paen[h];
            
            if( ((k = fs_find(p, FS_FIRST)) != 1) ||
                (!p->isdir) )
            {
                lcd_cputs(fs_error);
                keybd_getc();
                return 0xFF;
            }

            fs_base_ptr = p->start;

            //пишем имя каталога
            for(k = 0; k < 15; ++k)
            {
                if(!p->name[k])
                {
                    break;
                }
            }
            p->name[k] = '/';
            p->name[k+1] = 0;
            lcd_puts(p->name);
        }

        //найти первый файл
        fs_read_ptr = fs_base_ptr;
        fs_file_ptr = 0;

        if( (!fs_seek(cuen[h], 0)) ||
            ((k = fs_find(p, FS_CUR)) == 0xFF) )
        {
            lcd_clear(1);
            lcd_clear(0);
            lcd_cputs(fs_error);
            keybd_getc();
            return 0xFF;
        }

        o = 0;
        while(!o)
        {
            //пишем имя файла
            lcd_clear(1);

            if(!k)
            {
                lcd_cputs(fs_empty);
            }
            else
            {
                if(p->isdir)
                {
                    for(k = 0; k < 15; ++k)
                    {
                        if(!p->name[k])
                        {
                            break;
                        }
                    }
                    p->name[k] = '/';
                    p->name[k+1] = 0;
                }
                lcd_puts(p->name);
            }
            
            //команда
            b = 0;
            while(1)
            {
                if(k)
                {
                    temp1 = fs_read_ptr;
                    temp2 = fs_file_ptr;
                    b = p->isdir;
                }
                
                k = 0;
            
                switch(keybd_getc())
                {
                case KEY_UP:
                    if( (k = fs_find(p, FS_PREV)) == 0xFF )
                    {
                        lcd_clear(1);
                        lcd_clear(0);
                        lcd_cputs(fs_error);
                        keybd_getc();
                        return 0xFF;
                    }
                    break;
                case KEY_DOWN:
                    if( (k = fs_find(p, FS_NEXT)) == 0xFF )
                    {
                        lcd_clear(1);
                        lcd_clear(0);
                        lcd_cputs(fs_error);
                        keybd_getc();
                        return 0xFF;
                    }
                    break;
                case KEY_RIGHT:
                    if( (b) && (h < LENGTH(paen)-1) )
                    {
                        cuen[h] = temp2;
                        h++;
                        paen[h] = temp1;
                        cuen[h] = 0;
                        o = 1;
                    }
                    break;
                case KEY_LEFT:
                    if(h > 0)
                    {
                        h--;
                        o = 1;
                    }
                    break;
                case KEY_ENTER:
                    if( ((b) && (mode & FS_FOLDER)) ||
                        ((!b) && (mode & FS_FILE)) )
                    {
                        if( (fs_seek(temp2, 0) != 1) ||
                            (fs_find(p, FS_CUR) != 1) )
                        {
                            return 0xFF;
                        }
                        return 1;
                    }
                    break;
                case KEY_BACK:
                    return 0;
                }
                
                if(k || o)
                {
                    break;
                }
                
                fs_read_ptr = temp1;
                fs_file_ptr = temp2;
            }
        }
    }
}

//------------------------------------------------
