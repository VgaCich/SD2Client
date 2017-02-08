/*
*   обработка клавы
*/

//--------------------------------------------------------------------

#include "keyboard.h"

//--------------------------------------------------------------------

volatile __tiny byte keybd_state = 0;
volatile __tiny byte keybd_async = 0;
volatile __tiny byte keybd_cycle = 0;
volatile __tiny byte keybd_shift = 0;

volatile __tiny byte bklit_cntrl = 0;
volatile __tiny byte bklit_cycle = 0;

//--------------------------------------------------------------------

#pragma vector=TIMER0_OVF_vect
__interrupt void TIMER0_OVF(void)
{
    byte t;

    t = keybd_cycle++;

    if( (t & 3) >= bklit_cntrl )
    {
        BKLIT_PORT |= BKLIT_EN;
    }
    else
    {
        BKLIT_PORT &= ~BKLIT_EN;
    }

    if(t == 0xFF)
    {
        if(bklit_cycle >= BKLIT_TURNOFF_TIME)
        {
            LED_PORT &= ~(LED_RED | LED_GRN | LED_BLU);
            bklit_cntrl |= 0x80;
        }
        else
        {
            bklit_cycle++;
        }
    }

    if( !(t & 1) )
    {
        t = (t >> 1) & 0x07;
    
        //такт0, устанавливаем RESET
        if(t == 0)
        {    
            KEYBD_RESET_PORT |= KEYBD_RESET;
            return;
        }
    
        //такт1, сбрасываем RESET
        if(t == 1)
        {
            keybd_shift = 1;
            KEYBD_RESET_PORT &= ~KEYBD_RESET;
            return;
        }
    
        //чётный такт, устанавливаем CLK
        if((t & 1) == 0)
        {
            KEYBD_PORT |= KEYBD_CLOCK;
        }
        else //нечётный такт, сбрасываем CLK
        {
            KEYBD_PORT &= ~KEYBD_CLOCK;
        
            //читаем канал 0
            if(KEYBD_PIN & KEYBD_INPUT0)
            {
                if(! (keybd_state & keybd_shift))
                {
                    keybd_async |= keybd_shift;
                    keybd_state |= keybd_shift;

                    bklit_cntrl &= ~0x80;
                    bklit_cycle = 0;
                }
            }
            else
            {
                keybd_state &= ~keybd_shift;
            }
            keybd_shift <<= 1;
        
            //читаем канал 1
            if(KEYBD_PIN & KEYBD_INPUT1)
            {
                if(! (keybd_state & keybd_shift))
                {
                    keybd_async |= keybd_shift;
                    keybd_state |= keybd_shift;

                    bklit_cntrl &= ~0x80;
                    bklit_cycle = 0;
                }
            }
            else
            {
                keybd_state &= ~keybd_shift;
            }
            keybd_shift <<= 1;
        }
    }
}

//--------------------------------------------------------------------

void bklit_on(void)
{
    bklit_cntrl &= ~0x80;
    bklit_cycle = 0;
}

//--------------------------------------------------------------------

void keybd_init(void)
{
    TCCR0 |= 0x04; // CLK/256
    TIMSK |= 0x01; // TOIE0
}

//--------------------------------------------------------------------

byte keybd_getc(void)
{
    byte k;

    while(!keybd_async)
    {
        lcd_bat_glyph();
    }
    k = keybd_async;
    keybd_async = 0;
    return k;
}

//--------------------------------------------------------------------
