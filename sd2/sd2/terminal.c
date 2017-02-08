/*
*   функции в/в через UART
*/

//--------------------------------------------------------------------

#include "terminal.h"

//--------------------------------------------------------------------

volatile __tiny byte rxbuf [RXBUF_SIZE];
volatile __tiny byte rx_wp = 0;
volatile __tiny byte rx_rp = 0;

//--------------------------------------------------------------------

#pragma vector=UART_RXC_vect
__interrupt void UART_RXC(void)
{
    byte t, c;
    //примем байт
    c = UDR;
    //нов. указатель
    t = (rx_wp + 1) & (RXBUF_SIZE - 1);
    //нет места в буф?
    if(t == rx_rp)
    {
        return;
    }
    //отправим принятый байт в буф
    rxbuf[rx_wp] = c;
    rx_wp = t;
}

//--------------------------------------------------------------------

char term_getc(void)
{
    byte c;
    //пододжём данных в буф.
    while(rx_wp == rx_rp)
        ;
    //вытащим байт из буф.
    c = rxbuf[rx_rp];
    rx_rp = (rx_rp + 1) & (RXBUF_SIZE - 1);
    //вернём байт
    return c;
}

//--------------------------------------------------------------------

void term_putc(char c)
{
    //подождём окончания в/в (URDE=1)
    while(!(UCSRA & 0x20))
        ;
    //отправим байт
    UDR = c;
}

//--------------------------------------------------------------------

void term_cputs(const __flash char s[])
{
    while(*s)
    {
        term_putc( *(s++) );
    }
}

//--------------------------------------------------------------------

char term_chkbuf(void)
{
    return (rx_rp != rx_wp);
}

//--------------------------------------------------------------------

void term_clrbuf(void)
{
    rx_rp = rx_wp;
}

//--------------------------------------------------------------------

byte term_get_string(char *buf, byte sz)
{
    int i;
    char c;
    for(i = 0; i < sz; ++i)
    {
        if( (c = term_getc()) == 0 )
            break;
        buf[i] = c;
    }
    buf[i] = 0;
    return i;
}

//--------------------------------------------------------------------

void term_put_string(char *s)
{
    char c;
    while( (c = *(s++)) != 0 )
    {
        term_putc(c);
    }
}

//--------------------------------------------------------------------

void term_init(void)
{
    UCSRA = TERM_U2X << 1; // U2X = TERM_U2X
                           // MPCM = 0

    UCSRB = 0x98;   // RXCIE = 1 (rxc int enabled)
                    // TXCIE = 0 (no txc int)
                    // UDRIE = 0 (no urde int)
                    // RXEN = 1 (enable rx)
                    // TXEN = 1 (enable tx)
                    // UCSZ2 = 0 (no 9-bit char)

    UBRRH = 0x86;   // UMSEL = 0 (async operation)
                    // UPM = 00 (no parity)
                    // UBSS = 0 (1 stop bit)
                    // UCSZ = 11 (8 bit)
                    // UCPOL = 0 (for sync mode only)

    UBRRL = TERM_UBRR & 255;
    UBRRH = TERM_UBRR >> 8;
}

//--------------------------------------------------------------------
