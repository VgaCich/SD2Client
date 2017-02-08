/*
*    функции в/в через SPI
*/

//------------------------------------------------

#include "spi.h"

//------------------------------------------------

byte spi_putc(byte c)
{
    SPDR = c;
    while(!(SPSR & 0x80))
        ;
    c = SPDR;
    return c;
}

//------------------------------------------------

void spi_init(byte speed)
{
    SPCR = 0x58 | (speed & 3);  // SPIE=0, no spi int
                                // SPE=1, SPI enabled
                                // DORD=0, fucked lame data order
                                // MSTR=1, master
                                // CPOL=1, inverted SCK line
                                // CPHA=0, front=setup fall=sample
    SPSR = (speed >> 2) & 1;
}

//------------------------------------------------
