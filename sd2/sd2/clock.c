/*
*   функции задержки
*/

//--------------------------------------------------------------------

#include "clock.h"

//--------------------------------------------------------------------

#pragma optimize=0
void delay_us(byte latency)
{
    __delay_cycles(2);
    while(--latency)
    {
        __delay_cycles(1);
    }
}

//--------------------------------------------------------------------

#pragma optimize=0
void delay_us_w(word latency)
{
    byte i;
    delay_us( (byte)latency );
    for(i = ((byte*)(&latency))[1]; i; --i)
    {
        __delay_cycles(4085);
    }
 
}

//--------------------------------------------------------------------

#pragma optimize=0
void delay_ms(word latency)
{
    while(latency--)
    {
      delay_us_w(1000);
    }
}

//--------------------------------------------------------------------
