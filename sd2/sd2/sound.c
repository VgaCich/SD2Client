//------------------------------------------------

#include "sound.h"

//------------------------------------------------

fchar glyph_pause[8] = "\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x00";
fchar glyph_dots[8]  = "\x00\x00\x00\x00\x00\x00\x00\x15";

//------------------------------------------------

fstring snd_info_mask = "? \x02 ?x?? ??:?? \x03";

//------------------------------------------------

volatile __tiny byte sound_buf_A [SOUND_BUF_SIZE];
volatile __tiny byte sound_buf_B [SOUND_BUF_SIZE];
volatile __tiny byte snd_w = 0;
volatile __tiny byte snd_r = 0;
volatile __tiny byte snd_dup = 0;
volatile __tiny word snd_freq = 0;

//------------------------------------------------

volatile __tiny byte ima_step_index_A;
volatile __tiny byte ima_step_index_B;
volatile __tiny short ima_predictor_A;
volatile __tiny short ima_predictor_B;

//------------------------------------------------

volatile __tiny byte led_presc;
volatile __tiny byte led_timer;
volatile __tiny word led_gathr;

//------------------------------------------------

const __tinyflash signed char ima_index_table[8] =
{
    -1, -1, -1, -1, 2, 4, 6, 8
};

//------------------------------------------------

const __flash word ima_step_table[89] =
{
     7,     8,     9,    10,    11,    12,    13,    14,
    16,    17,    19,    21,    23,    25,    28,    31,
    34,    37,    41,    45,    50,    55,    60,    66,
    73,    80,    88,    97,   107,   118,   130,   143,
   157,   173,   190,   209,   230,   253,   279,   307,
   337,   371,   408,   449,   494,   544,   598,   658,
   724,   796,   876,   963,  1060,  1166,  1282,  1411,
  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,
  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,
  7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
 32767
};

//------------------------------------------------

#pragma vector=TIMER1_OVF_vect
__interrupt void TIMER1_OVF(void)
{
    byte k, t, l, r;

    snd_dup ^= 2;
    if(snd_dup == 3)
    {
        return;
    }

    k = snd_r;
    if(k != snd_w)
    {
        l = sound_buf_A[k];
        r = sound_buf_B[k];
        OCR1A = ((word)l) + 30;
        OCR1B = ((word)r) + 30;
        
        
        snd_r = (k + 1) & (SOUND_BUF_SIZE - 1);

        if(!(bklit_cntrl & 0x80))
        {
            if(!led_presc)
            {
                t = (l >> 1) + (r >> 1) - 0x80;
                if(t >= 0x80) t = 0xff - t;
                led_gathr += t;
                
                if(!led_timer)
                {
                    LED_PORT &= ~(LED_RED | LED_GRN | LED_BLU);
    
                    t = (((byte*)(&led_gathr))[1]) >> 3;
                    if(t > 7) t = 7;
    
                    switch(t)
                    {
                    case 1: LED_PORT |= (LED_RED); break;
                    case 2: LED_PORT |= (LED_RED|LED_BLU); break;
                    case 3: LED_PORT |= (LED_RED|LED_GRN); break;
                    case 4: LED_PORT |= (LED_GRN); break;
                    case 5: LED_PORT |= (LED_BLU); break;
                    case 6: LED_PORT |= (LED_GRN|LED_BLU); break;
                    case 7: LED_PORT |= (LED_RED|LED_GRN|LED_BLU); break;
                    }
                    led_gathr = 0;
                }
                led_timer++;
                
                led_presc = 3;
            }
            led_presc--;
        }
    }
}

//------------------------------------------------

void sound_init(void)
{
    lcd_load_cgram(2, glyph_pause);
    lcd_load_cgram(4, glyph_dots);
}

//------------------------------------------------

void sound_start(byte dup, word freq)
{
    snd_dup = dup;
    snd_freq = freq;

    ICR1 = (CLK+freq/2) / freq;

    OCR1A = 30;
    OCR1B = 30;

    TCCR1A = 0xA2;  //COM1A 1:0 = 2
                    //COM1B 1:0 = 2
                    //FOC1A = 0
                    //FOC1B = 0
                    //WGM1 3:0 = 14

    TCCR1B = 0x19;  //ICNC1 = 0
                    //ICES1 = 0
    TIMSK |= 0x04;  //TOIE1 = 1
}

//------------------------------------------------

void sound_stop(void)
{
    TIMSK &= ~0x04;
    TCCR1A = 0;
    TCCR1B = 0;
}

//------------------------------------------------

void play_PCM8_mono_sector(word count)
{
    byte k;
    byte w, t;
    
    w = snd_w;
    
    while(count)
    {
        k = spi_putc(0xFF);
        
        t = (w + 1) & (SOUND_BUF_SIZE - 1);
        while(t == snd_r)
            ;

        sound_buf_A[w] = k;
        sound_buf_B[w] = k;
        snd_w = w = t;
        
        count--;
    }
}

//------------------------------------------------

void play_PCM8_stereo_sector(word count)
{
    byte k1, k2;
    byte w, t;
    
    w = snd_w;
    
    while(count >= 2)
    {
        k1 = spi_putc(0xFF);
        k2 = spi_putc(0xFF);
        
        t = (w + 1) & (SOUND_BUF_SIZE - 1);
        while(t == snd_r)
            ;

        sound_buf_A[w] = k1;
        sound_buf_B[w] = k2;
        snd_w = w = t;
        
        count -= 2;
    }
    
    if(count)
    {
        spi_putc(0xFF);
    }
}

//------------------------------------------------

void play_IMA_mono_sector(word count)
{
    byte k;
    byte s1, s2;
    byte w, t;

    signed char step_index;
    long predictor;
    word step;
    word diff;

    w = snd_w;

    step_index = ima_step_index_A;
    predictor = (long)ima_predictor_A;

    while(count)
    {
        k = spi_putc(0xFF);

        step = ima_step_table[step_index];
        step_index += ima_index_table[k & 7];
        if(step_index < 0) { step_index = 0; }
        if(step_index > 88) { step_index = 88; }
        diff = 0;
        if(k & 4) { diff += step; } step >>= 1;
        if(k & 2) { diff += step; } step >>= 1;
        if(k & 1) { diff += step; } step >>= 1;
        diff += step;
        if(k & 8)
        {
            predictor -= (unsigned long)diff;
            if(predictor < -32768) { predictor = -32768; }
        }
        else
        {
            predictor += (unsigned long)diff;
            if(predictor > 32767) { predictor = 32767; }
        }
        s1 = ((byte*)(&predictor))[1] + 0x80;

        t = (w + 1) & (SOUND_BUF_SIZE - 1);
        while(t == snd_r)
            ;
        sound_buf_A[w] = s1;
        sound_buf_B[w] = s1;
        snd_w = w = t;

        k >>= 4;

        step = ima_step_table[step_index];
        step_index += ima_index_table[k & 7];
        if(step_index < 0) { step_index = 0; }
        if(step_index > 88) { step_index = 88; }
        diff = 0;
        if(k & 4) { diff += step; } step >>= 1;
        if(k & 2) { diff += step; } step >>= 1;
        if(k & 1) { diff += step; } step >>= 1;
        diff += step;
        if(k & 8)
        {
            predictor -= (unsigned long)diff;
            if(predictor < -32768) { predictor = -32768; }
        }
        else
        {
            predictor += (unsigned long)diff;
            if(predictor > 32767) { predictor = 32767; }
        }
        s2 = ((byte*)(&predictor))[1] + 0x80;
        
        t = (w + 1) & (SOUND_BUF_SIZE - 1);
        while(t == snd_r)
            ;
        sound_buf_A[w] = s2;
        sound_buf_B[w] = s2;
        snd_w = w = t;

        count--;
    }

    ima_step_index_A = step_index;
    ima_predictor_A = (short)predictor;
}

//------------------------------------------------

void play_IMA_stereo_sector(word count)
{
    byte k;
    byte s1, s2;
    byte w, t;
    
    signed char step_index_A;
    signed char step_index_B;
    long predictor_A;
    long predictor_B;
    word step;
    word diff;

    w = snd_w;
    
    step_index_A = ima_step_index_A;
    predictor_A = (long)ima_predictor_A;
    step_index_B = ima_step_index_B;
    predictor_B = (long)ima_predictor_B;

    while(count)
    {
        k = spi_putc(0xFF);

        step = ima_step_table[step_index_A];
        step_index_A += ima_index_table[k & 7];
        if(step_index_A < 0) { step_index_A = 0; }
        if(step_index_A > 88) { step_index_A = 88; }
        diff = 0;
        if(k & 4) { diff += step; } step >>= 1;
        if(k & 2) { diff += step; } step >>= 1;
        if(k & 1) { diff += step; } step >>= 1;
        diff += step;
        if(k & 8)
        {
            predictor_A -= (unsigned long)diff;
            if(predictor_A < -32768) { predictor_A = -32768; }
        }
        else
        {
            predictor_A += (unsigned long)diff;
            if(predictor_A > 32767) { predictor_A = 32767; }
        }
        s1 = ((byte*)(&predictor_A))[1] + 0x80;
        
        k >>= 4;

        step = ima_step_table[step_index_B];
        step_index_B += ima_index_table[k & 7];
        if(step_index_B < 0) { step_index_B = 0; }
        if(step_index_B > 88) { step_index_B = 88; }
        diff = 0;
        if(k & 4) { diff += step; } step >>= 1;
        if(k & 2) { diff += step; } step >>= 1;
        if(k & 1) { diff += step; } step >>= 1;
        diff += step;
        if(k & 8)
        {
            predictor_B -= (unsigned long)diff;
            if(predictor_B < -32768) { predictor_B = -32768; }
        }
        else
        {
            predictor_B += (unsigned long)diff;
            if(predictor_B > 32767) { predictor_B = 32767; }
        }
        s2 = ((byte*)(&predictor_B))[1] + 0x80;
        
        t = (w + 1) & (SOUND_BUF_SIZE - 1);
        while(t == snd_r)
            ;
        sound_buf_A[w] = s1;
        sound_buf_B[w] = s2;
        snd_w = w = t;

        count--;
    }

    ima_step_index_A = step_index_A;
    ima_predictor_A = (short)predictor_A;
    ima_step_index_B = step_index_B;
    ima_predictor_B = (short)predictor_B;
}

//------------------------------------------------

void sound_info(byte tag)
{
    lcd_gotoxy(0, 0);
    lcd_cputs(snd_info_mask);

    lcd_gotoxy(0, 0);
    lcd_putc(tag);

    lcd_gotoxy(4, 0);
    switch(tag & 2)
    {
    case 0: lcd_putc('1'); break;
    case 2: lcd_putc('2'); break;
    }
    
    lcd_gotoxy(6, 0);
    lcd_putb((snd_freq+500) / 1000, 2);
}

//------------------------------------------------

void sound_duration(word t)
{
    lcd_gotoxy(9, 0);
    lcd_putb(t / 60, 2);

    lcd_gotoxy(12, 0);
    lcd_putb(t % 60, 2);
}

//------------------------------------------------

void sound_show_paused(byte p)
{
    lcd_gotoxy(2, 0);
    lcd_putc(p + 1);
}

//------------------------------------------------

byte play_file(direntry *d)
{
    dword start, size, bps, tmp;
    word tail, count;
    byte tag, pad;
    byte result = 0;
    byte ctr = 0;
    byte paused = 0;
    byte dup = 0;
    byte nlen;

    ima_predictor_A = 0;
    ima_predictor_B = 0;
    ima_step_index_A = 0;
    ima_step_index_B = 0;
    
    size = d->size;
    start = d->start;
    
    if(size > 2)
    {
        //читаем сектор
        if(!sd_prep_read_sector(start))
        {
            LED_PORT &= ~(LED_RED | LED_GRN | LED_BLU);
            return 0xFF;
        }
        tail = 508;
        
        //читаем тег
        tag = spi_putc(0xFF);
        pad = spi_putc(0xFF);
        size -= 2;
        tail -= 2;

        //воспроизводим трек
        if( ((tag & (~7)) == 'P') &&
            ((pad == 0) || ((pad>='0') && (pad<='8'))) )
        {
            result = 1;
            
            if(!pad)
            {
                pad = (tag & 1) ? '6' : '3';
            }

            //байт в сек
            switch(pad)
            {
            case '0':
                dup = 1;
                bps = 16000;
                break;

            case '1':
                dup = 1;
                bps = 22050;
                break;

            case '2':
                dup = 1;
                bps = 24000;
                break;

            case '3':
                dup = 1;
                bps = 32000;
                break;

            case '4':
                dup = 0;
                bps = 22050;
                break;

            case '5':
                dup = 0;
                bps = 24000;
                break;

            case '6':
                dup = 0;
                bps = 32000;
                break;
                
            case '7':
                dup = 0;
                bps = 44100;
                break;

            case '8':
                dup = 0;
                bps = 48000;
                break;
            }

            tmp = bps;
            if(dup)     tmp /= 2;
            if(tag & 2) tmp *= 2;
            if(tag & 4) tmp /= 2;

            sound_start(dup, bps);
            sound_info(tag);
            sound_show_paused(0);
            lcd_clear(1);
            lcd_putsn(d->name, 16);
            for(nlen = 0; d->name[nlen]; nlen++)
                ;
            if(nlen > 16)
            {
                lcd_gotoxy(15,1);
                lcd_putc('\x04');
            }

            tag &= ~1;        

            while(size)
            {
                //нужно взять новый сектор?
                if(!tail)
                {
                    //дочитываем старый сектор
                    ((byte*)(&start))[0] = spi_putc(0xFF);
                    ((byte*)(&start))[1] = spi_putc(0xFF);
                    ((byte*)(&start))[2] = spi_putc(0xFF);
                    ((byte*)(&start))[3] = spi_putc(0xFF);
                    spi_putc(0xFF);
                    spi_putc(0xFF);
                    sd_end_read_sector();

                    //конец цепочки
                    if(start == 0xFFFFFFFF)
                    {
                        sound_stop();
                        LED_PORT &= ~(LED_RED | LED_GRN | LED_BLU);
                        return 1;
                    }
                    
                    //кнопка нажата
                    if(keybd_async)
                    {
                        do {
                            while(!keybd_async)
                                ;
                            switch(keybd_async)
                            {
                                case KEY_DOWN:
                                    keybd_async = 0;
                                case KEY_BACK:
                                case KEY_UP:
                                    sound_stop();
                                    LED_PORT &= ~(LED_RED | LED_GRN | LED_BLU);
                                    return 1;

                                case KEY_ENTER:
                                    paused = !paused;
                                    sound_show_paused(paused);
                                    LED_PORT &= ~(LED_RED | LED_GRN | LED_BLU);
                                    break;
                                    
                                case KEY_LEFT:
                                case KEY_RIGHT:
                                    break;
                                    
                            }
                            keybd_async = 0;
                        } while(paused);
                    }
                    
                    //выводим время
                    if(!(ctr & 0x1F))
                    {
                        sound_duration(size / tmp);
                        lcd_bat_glyph();
                    }
                    
                    ctr++;

                    //запрашиваем новый сектор
                    start <<= 9;
                    if(!sd_prep_read_sector(start))
                    {
                        sound_stop();
                        LED_PORT &= ~(LED_RED | LED_GRN | LED_BLU);
                        return 0xFF;
                    }
                    tail = 508;
                }

                //сколько считывать
                if(size >= (dword)tail)
                {
                    count = tail;
                }
                else
                {
                    count = (word)size;
                }
                
                //воспроизводим сектор
                switch(tag)
                {
                case 'V': play_IMA_stereo_sector(count); break;
                case 'T': play_IMA_mono_sector(count); break;
                case 'R': play_PCM8_stereo_sector(count); break;
                case 'P': play_PCM8_mono_sector(count); break;
                }
                
                //счииали
                size -= (dword)count;
                tail -= count;
            }
            
            sound_stop();
        }
        
        //дочитываем конец сектора
        tail += 6;
        while(tail)
        {
            spi_putc(0xFF);
            tail--;
        }

        //закрываем чтение сектора
        sd_end_read_sector();
    }
    LED_PORT &= ~(LED_RED | LED_GRN | LED_BLU);
    return result;
}

//------------------------------------------------

fstring s_error  = "* Ошибка *";
fstring s_empty  = "* Пусто *";
fstring s_full   = "* Уже полон *";
fstring s_player = "Музыка";
fstring s_play   = "Воспроизвести";
fstring s_insert = "Добавить";
fstring s_remove = "Удалить";
fstring s_clear  = "Очистить";

const __flash fchar* pls_menu_items[] =
{
    s_play,
    s_insert,
    s_remove,
    s_clear,
    0
};

//------------------------------------------------

void player(void)
{
    direntry d, t;
    dword pls [96];
    byte i, k;
    byte cnt = 0, cur = 0, dir = 0;

    for(i = 0; i < LENGTH(pls); ++i) { pls[i] = 0; }

    while(1)
    {
        lcd_clear(0);
        lcd_cputs(s_player);

        if(!cnt)
        {
            lcd_clear(1);
            lcd_cputs(s_empty);
        }
        else
        {
            i = cur;

            while(1)
            {
                if(dir == 1)
                {
                    i = (i + 1) % LENGTH(pls);
                }
                if(dir == 2)
                {
                    i = (i + LENGTH(pls) - 1) % LENGTH(pls);
                }
                if(i >= LENGTH(pls))
                {
                    break;
                }

                if(pls[i])
                {
                    fs_open_entry(pls[i]);
                    if( (k = fs_find(&t, FS_CUR)) == 0xFF)
                    {
                        lcd_clear(1);
                        lcd_cputs(s_error);
                        keybd_getc();
                        return;
                    }

                    d = t;
                    cur = i;
                    break;
                }

                if(dir == 0)
                {
                    i++;
                }
                if(i >= LENGTH(pls))
                {
                    break;
                }
            }
        }
        dir = 0;

        if(cnt)
        {
            for(k = 0, i = 0; i <= cur; ++i)
            {
                if(pls[i])
                {
                    k++;
                }
            }

            lcd_gotoxy(11, 0);
            lcd_putb(k, 2);
            lcd_putc('/');
            lcd_putb(cnt, 2);
        }

        while(1)
        {
            if(cnt)
            {
                lcd_clear(1);
                menu_line(d.name, 1, 0, 15);
            }
        
            k = keybd_getc();

            if(k == KEY_UP)
            {
                dir = 2;
                break;
            }
            else if(k == KEY_DOWN)
            {
                dir = 1;
                break;
            }
            else if(k == KEY_ENTER)
            {
                if(!cnt)
                {
                    k = 1;
                }
                else
                {
                    k = menu(s_player, pls_menu_items, 0);
                }

                if(k == 0)
                {
                    i = cur;
                    while(1)
                    {
                        if(pls[i])
                        {
                            fs_open_entry(pls[i]);
                            if(fs_find(&t, FS_CUR) == 1)
                            {
                                bklit_on();
                                if( (k = play_file(&t)) == 0xFF)
                                {
                                    lcd_clear(1);
                                    lcd_cputs(s_error);
                                    keybd_getc();
                                    return;
                                }
                            }
                            if(keybd_async == KEY_BACK)
                            {
                                keybd_async = 0;
                                break;
                            }
                            if(keybd_async == KEY_UP)
                            {
                                for(k = i - 1; k != i; --k)
                                {
                                    if(k == 0xFF) { k = LENGTH(pls) - 1; }
                                    if(pls[k]) { break; }
                                }
                                i = k - 1;
                            }

                            keybd_async = 0;
                        }
                        i++;
                        if(i == LENGTH(pls)) { i = 0; }
                    }
                    cur = i;
                }
                else if(k == 1)
                {
                    if(cnt == LENGTH(pls))
                    {
                        lcd_clear(1);
                        lcd_cputs(s_full);
                        keybd_getc();
                        break;
                    }

                    fs_browse(&t, FS_FILE | FS_FOLDER);

                    if(!(t.isdir))
                    {
                        i = cur;
                        do
                        {
                            if(!pls[i])
                            {
                                pls[i] = t.entry;
                                cur = i;
                                cnt++;
                                break;
                            }

                            i++;
                            if(i >= LENGTH(pls)) { i = 0; }
                        } while(i != cur);
                    }
                    else
                    {
                        fs_open_entry(t.start);

                        if(fs_find(&t, FS_CUR) == 1)
                        {
                            do
                            {
                                if(!t.isdir)
                                {
                                    i = cur;
                                    do
                                    {
                                        if(!pls[i])
                                        {
                                            pls[i] = t.entry;
                                            cur = i;
                                            cnt++;
                                            break;
                                        }
            
                                        i++;
                                        if(i >= LENGTH(pls)) { i = 0; }
                                    } while(i != cur);
                                }
                            } while(fs_find(&t, FS_NEXT) == 1);
                        }
                    }
                }
                else if(k == 2)
                {
                    if(pls[cur] != 0)
                    {
                        pls[cur] = 0;
                        cnt--;

                        i = cur + 1;
                        while(i != LENGTH(pls))
                        {
                            if(pls[i])
                            {
                                break;
                            }
                            i++;
                        }

                        if(i == LENGTH(pls))
                        {
                            i = cur;
                            while(i != 0)
                            {
                                if(pls[i])
                                {
                                    break;
                                }
                                i--;
                            }
                        }

                        cur = i;
                    }
                }
                else if(k == 3)
                {
                    if(confirm(s_clear))
                    {
                        for(i = 0; i < LENGTH(pls); ++i)
                        {
                            pls[i] = 0;
                        }
                        cnt = 0;
                        cur = 0;
                    }
                }

                break;
            }
            else if(k == KEY_BACK)
            {
                return;
            }
        }
    }

}

//------------------------------------------------
