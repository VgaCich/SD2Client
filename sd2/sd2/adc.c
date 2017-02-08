//--------------------------------------------------------------------

#include "adc.h"

//--------------------------------------------------------------------

#define corr 246
volatile __tiny word volt = 0;
volatile __tiny byte adct = 0;

//--------------------------------------------------------------------

#pragma vector=ADC_vect
__interrupt void ADC_int(void)
{
    word v;
    
    if(!adct)
    {
        v = ADC;
        v = ((dword)v * (dword)corr) / 256ul;
        
        switch(ADMUX & 0x07)
        {
        case 0:
            volt = v;
            break;
        }
    }
    adct++;
}

//--------------------------------------------------------------------

void adc_init(void)
{
    ADCSRA = 0x2B;
    ADMUX = 0xC0;
    SFIOR |= 0x80;
    DDRA &= 0xFE;
}

//--------------------------------------------------------------------

void adc_enable(void)
{
    ADCSRA |= 0x80;
    delay_ms(50);
    ADCSRA |= 0x40;
}

//--------------------------------------------------------------------

void adc_disable(void)
{
    ADCSRA &= ~0x7F;
}

//--------------------------------------------------------------------
