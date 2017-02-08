//------------------------------------------------

#include "rsfs.h"

//------------------------------------------------

fstring fs_empty = "* ��� ������ *";
fstring fs_error = "* ������! *";

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
*   ��� ����� ��������� -> 1
*   ��������� ������ -> 0xFF
*   ������ ���������� (����� �����) -> 0
*
*   ����������:
*       fs_base_ptr
*       fs_read_ptr
*
*   �������� (������ � ������ ������):
*       fs_read_ptr
*/

byte fs_skip(dword count)
{
    dword cuse;
    dword temp;
    
    //��� ����������
    if(count == 0)
    {
        return 1;
    }
    
    //����� ������ "��������" �������
    cuse = fs_read_ptr & ~0x1FF;
    
    //���������� ���� �� ������ "��������" �������
    count += fs_read_ptr & 0x1FF;

    //����� ���������� ������ (��� ���������� ��������)
    while(count >= 508)
    {
        //��������, ��� ���������� ������
        count -= 508;

        //����� ����� ���. ������� / ������ ����. �������
        temp = cuse + 508;

        //������ ������ ����. �������
        if(!sd_read( (byte*)(&cuse), temp, 4))
        {
            return 0xFF;
        }

        //����. ������� ���� (����� �����)
        if(cuse == 0xFFFFFFFF)
        {
            //��� ����� "����������"
            if(!count)
            {
                fs_read_ptr = temp;
                return 1;
            }
            
            //�������� "������������" �����
            return 0;
        }
        
        //������ -> �����
        cuse <<= 9;
    }

    //����� ������ ������� + "����������" �����
    fs_read_ptr = cuse + count;

    return 1;
}

//------------------------------------------------

/*
*   fs_seek
*
*   ������� �� ������ ����� -> 1
*   ������ ������� (�������� �� ������� �����) -> 0
*   ��������� ������ -> 0xFF
*
*   ����������:
*       fs_base_ptr
*       fs_read_ptr
*       fs_file_siz
*       fs_file_ptr
*
*   �������� (������ � ������ ������):
*       fs_read_ptr
*       fs_file_ptr
*/

byte fs_seek(dword offset, byte curor)
{
    byte k;
    
    //������� �����, ���� ���� �������
    if(curor)
    {
        offset += fs_file_ptr;
    }

    //������� �� ������� ����� (����� ��� �����)
    if( (offset > 0x7FFFFFFF) || 
        (offset > fs_file_siz) )
    {
        return 0;
    }
    
    //����� ������� "�����" (��������� ��� �����������)
    if(offset > fs_file_ptr)
    {
        if( (k = fs_skip(offset - fs_file_ptr)) == 1 )
        {
            fs_file_ptr = offset;
        }
        return k;
    }

    //����� ��������� "�����"
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
*   � ������ ������ -> ���-�� �������� ����
*   ��������� ������ -> 0xFF
*
*   ����������:
*       fs_read_ptr
*       fs_file_siz
*       fs_file_ptr
*
*
*   �������� (������ � ������ ������):
*       fs_read_ptr
*       fs_file_ptr
*/

byte fs_read(byte *buf, byte count)
{
    dword filetail; //"�����" �����
    dword temp;     //������ �����
    dword cupo;     //������ ������
    word tail;      //"�����" �������
    byte cb;        //������� ����� ������

    //������� "�����" �����
    filetail = fs_file_siz - fs_file_ptr;

    //������ ����� �� ������ "������"
    if( (dword)count > filetail )
    {
        count = (byte)filetail;
    }

    //��������� �����������
    if(count == 0)
    {
        return 0;
    }

    //������� ����� ���������
    cb = count;

    //������ ����� ������
    cupo = fs_read_ptr;

    //������� "�����" �������
    tail = (word)(508 - (cupo & 0x1FF)); //sector's tail

    //����� �������� ������ � "������" ���������
    if(count >= (word)tail)
    {
        //������ ����� �������
        if(sd_read(buf, cupo, (byte)tail) == 0)
        {
            return 0xFF;
        }

        //����� ������� ����. ������� / ����� ���.�������
        temp = (cupo & ~0x1FF) + 508;
        if( sd_read( (byte*)(&cupo), temp, 4) == 0 )
        {
            return 0xFF;
        }

        //����. ������� ��� (���� ����������)?
        if(cupo == 0xFFFFFFFF)
        {
            fs_read_ptr = temp;
            fs_file_ptr += (dword)tail;
            return (byte)tail;
        }

        //������ -> �����
        cupo <<= 9;

        //�������� ���������� �����
        count -= (byte)tail;
        buf += (byte)tail;
    }

    //��������� �����������
    if(count > 0)
    {
        //������ ������� ������
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
*   ����� -> 1
*   ������� -> 0
*   ��������� ������ -> 0xFF
*/

byte fs_find(direntry *p, byte mode)
{
    dword temp;
    byte k;

    if(mode == FS_ROOT)
    {
        //������� � �������� ��������
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
        //��� �����?
        if(mode == FS_PREV)
        {
            if( (k = fs_seek((dword)(-254), 1)) != 1)
            {
                return k;
            }
        }
        
        //������ ������� ��������
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
        
        //�����?
        if(p->isdir != 0xFF)
        {
            break;
        }
        
        //��� �����?
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
*   ����� -> 1
*   ������� -> 0
*   ��������� ������ -> 0xFF
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
            //������� � ��������� ��������
            if(fs_find(p, FS_ROOT) == 0xFF)
            {
                lcd_cputs(fs_error);
                keybd_getc();
                return 0xFF;
            }

            //����� ��� ��������
            lcd_putc('/');
        }
        else
        {
            //������� � �������� paen[h]
            fs_base_ptr = paen[h];
            
            if( ((k = fs_find(p, FS_FIRST)) != 1) ||
                (!p->isdir) )
            {
                lcd_cputs(fs_error);
                keybd_getc();
                return 0xFF;
            }

            fs_base_ptr = p->start;

            //����� ��� ��������
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

        //����� ������ ����
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
            //����� ��� �����
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
            
            //�������
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
