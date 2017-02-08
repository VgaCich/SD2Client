/*
*    ���������� SD ���������
*/

//------------------------------------------------

#include "sd.h"

//------------------------------------------------

void sd_cmd(byte cmd, dword op)
{
    //������� ���������� �����
    while(spi_putc(0xFF) != 0xFF)
        ;

    //�������� �������
    spi_putc(cmd | 0x40);
    
    //�������� ��������
    spi_putc( ((byte*)(&op)) [3] );
    spi_putc( ((byte*)(&op)) [2] );
    spi_putc( ((byte*)(&op)) [1] );
    spi_putc( ((byte*)(&op)) [0] );
    
    //�������� crc ��� CMD0
    spi_putc(0x95); // crc
}

//------------------------------------------------

byte sd_get_resp(void)
{
    byte i, c;
    for(i = 0; i != 255; ++i)
    {
        if( (c = spi_putc(0xFF)) != 0xFF )
            break;
    }
    return c;
}

//------------------------------------------------

word sd_crc16(byte *buf, word cb)
{
    word crc = 0;
    byte i, c, p;

    while(cb--)
    {
        c = *(buf++);
        for(i = 0; i < 8; i++) 
        {
            p = ((byte*)&crc)[1];
            crc <<= 1;
            if( (p ^ c) & 0x80) 
            {
                crc ^= 0x1021;
            }
            c <<= 1;
        }  
    }
    return crc;
}

//------------------------------------------------

byte sd_init(void)
{
    byte c, i, sl, sh, status = 0;
    
    //����� "������ 72" �������� ���, ���� ����� �� �������
    SD_CS_PORT &= ~SD_CS;
    for(i = 0; i < 10; ++i)
    {
        spi_putc(0xFF);
    }

    //������� �����
    SD_CS_ASSERT();
    
    //������� ����� � ����� ���������� RESET
    //���� ����� �� ���������������� � �� 
    //������� � ����� SPI
    for(i = 0; i < 16; ++i)
    {
        sd_cmd(SD_RESET, 0);
        if( (c = sd_get_resp()) == 0x01 )
            break;
        delay_ms(10);
    }
    if(c != 0x01)
        goto deassert;
    
    //����� �������� START_INITIALIZATION,
    //���� ����� ��������� �� ����������������
    for(i = 0; i < 16; ++i)
    {
        sd_cmd(SD_START_INITIALIZATION, 0);
        if( (c = sd_get_resp()) == 0x00 )
            break;
        delay_ms(10);
    }
    if(c != 0x00)
        goto deassert;
    
    //�������� ������ ����� �� ������
    sd_cmd(SD_SEND_STATUS, 0);
    sh = sd_get_resp();
    sl = sd_get_resp();
    if( (sl == 0) || (sh == 0) )
        status = 1;

deassert:
    //��������� ����
    SD_CS_DEASSERT();
    spi_putc(0xFF);

    return status;
}

//------------------------------------------------

byte sd_prep_read_sector(dword addr)
{
    byte c;
    word i;
    
    //������� �����
    SD_CS_ASSERT();

    //��������� ������ ����� � 512 ����
    sd_cmd(SD_SET_BLOCK_SIZE, 512);
    if(sd_get_resp() != 0x00)
        goto deassert;

    //����� ������� ������
    sd_cmd(SD_READ_SINGLE_BLOCK, addr);
    if(sd_get_resp() != 0x00)
        goto deassert;

    //������� ����� ������ ������
    for(i = 0; i != 0xFFFF; ++i)
    {
        if( (c = sd_get_resp()) != 0xFF )
            break;
    }
    if( c == 0xFE )
        return 1;

deassert:
    //��������� ����
    SD_CS_DEASSERT();
    spi_putc(0xFF);

    return 0;
}

//------------------------------------------------

void sd_end_read_sector(void)
{
    SD_CS_DEASSERT();
    spi_putc(0xFF);
}

//------------------------------------------------

byte sd_prep_write_sector(dword addr)
{
    //������� �����
    SD_CS_ASSERT();

    //��������� ������ ����� � 512 ����
    sd_cmd(SD_SET_BLOCK_SIZE, 512);
    if(sd_get_resp() != 0x00)
        goto deassert;

    //�������� WRITE_SINGLE_BLOCK
    sd_cmd(SD_WRITE_SINGLE_BLOCK, addr);
    if(sd_get_resp() != 0x00)
        goto deassert;

    //�������� ����� ������ ������
    spi_putc(0xFE);
    return 1;

deassert:
    //��������� ����
    SD_CS_DEASSERT();
    spi_putc(0xFF);

    return 0;
}

//------------------------------------------------

byte sd_end_write_sector(void)
{
    byte c;
    byte status = 0;
    
    //������� ������
    c = sd_get_resp();
    switch(c & 0x1F)
    {
        case 0x0B:
            status = 2;
            break;
        case 0x05:
            status = 1;
            break;
    }

    //��������� ����
    SD_CS_DEASSERT();
    spi_putc(0xFF);

    return status;
}

//------------------------------------------------

byte sd_read_block(byte *buf, dword addr, byte n, byte verify)
{
    byte c, status = 0;
    word i, crc, k;
    
    //������� �����
    SD_CS_ASSERT();

    //��������� ������ ����� ��� ������
    sd_cmd(SD_SET_BLOCK_SIZE, n);
    if(sd_get_resp() != 0x00)
        goto deassert;

    //����� ������� ������
    sd_cmd(SD_READ_SINGLE_BLOCK, addr);
    if(sd_get_resp() != 0x00)
        goto deassert;

    //��� ����� ������...
    for(i = 0; i != 0xFFFF; ++i)
    {
        if( (c = sd_get_resp()) != 0xFF )
            break;
    }
    if( c != 0xFE )
        goto deassert;

    //��������� ������
    for(k = 0; k < n; ++k)
    {
        buf[k] = spi_putc(0xFF);
    }

    //��������� crc
    ((byte*)(&crc))[1] = spi_putc(0xFF);
    ((byte*)(&crc))[0] = spi_putc(0xFF);
    if(verify)
    {
        if(crc != sd_crc16(buf, n))
        {
            status = 2;
            goto deassert;
        }
    }
    
    status = 1;

deassert:
    //��������� ����
    SD_CS_DEASSERT();
    spi_putc(0xFF);

    return status;
}

//------------------------------------------------

dword sd_get_sector_count(void)
{
    byte c, read_bl_len, c_size_mult;
    byte buf [16];
    word i, crc, c_size;
    dword scnt = 0;
    
    //������� �����
    SD_CS_ASSERT();

    //�������� ������� ������ CSD
    sd_cmd(SD_SEND_CSD, 0);
    if(sd_get_resp() != 0x00)
        goto deassert;

    //������� ������
    for(i = 0; i != 0xFFFF; ++i)
    {
        if( (c = sd_get_resp()) != 0xFF )
            break;
    }
    if( c != 0xFE )
        goto deassert;

    //��������� ������
    for(i = 0; i < 16; ++i)
    {
        buf[i] = spi_putc(0xFF);
    }

    //��������� crc
    ((byte*)(&crc))[1] = spi_putc(0xFF);
    ((byte*)(&crc))[0] = spi_putc(0xFF);
    if(sd_crc16(buf, 16) != crc)
        goto deassert;

    //������������ ����� CSD ���� �������� � ������ �������
    read_bl_len =       (buf[5]  & 0x0F);            // [83:80]

    c_size      = (word)(buf[6]  & 0x03) << 10 |     // [73:62]
                  (word)(buf[7]        ) << 2 |
                  (word)(buf[8]  & 0xC0) >> 6;

    c_size_mult =       (buf[9]  & 0x03) << 1 |      // [49..47]
                        (buf[10] & 0x80) >> 7;

    //�������� ���������� ��������
    scnt = (dword)(c_size + 1) << (c_size_mult + read_bl_len - 7);

deassert:
    //��������� ����
    SD_CS_DEASSERT();
    spi_putc(0xFF);

    return scnt;
}

//------------------------------------------------

byte sd_read(byte *buf, dword addr, byte n)
{
    byte i, k;
    
    if(n == 0)
    {
        return 1;
    }
    
    for(i = 0; i < 4; ++i)
    {
        k = sd_read_block(buf, addr, n, 1);
        if(k == 0) { return 0; }
        if(k == 1) { return 1; }
    }

    return 0;
}

//------------------------------------------------
