//--------------------------------------------------------------------

#ifndef __keyboard_h__
#define __keyboard_h__

//--------------------------------------------------------------------

//#include <iom16.h>
//#include <inavr.h>
#include "common.h"
#include "clock.h"
#include "types.h"
#include "lcd.h"

//--------------------------------------------------------------------

#define KEYBD_RESET_PORT    PORTD
#define KEYBD_RESET         0x80

#define KEYBD_PORT          PORTB
#define KEYBD_CLOCK         0x10

#define KEYBD_PIN           PINB
#define KEYBD_INPUT0        0x04
#define KEYBD_INPUT1        0x08

//--------------------------------------------------------------------

#define BKLIT_PORT          PORTD
#define BKLIT_EN            0x40

#define BKLIT_TURNOFF_TIME  45

//--------------------------------------------------------------------

#define KEYBD_KEY0          0x01
#define KEYBD_KEY1          0x02
#define KEYBD_KEY2          0x04
#define KEYBD_KEY3          0x08
#define KEYBD_KEY4          0x10
#define KEYBD_KEY5          0x20

//--------------------------------------------------------------------

#define KEY_LEFT            KEYBD_KEY0
#define KEY_RIGHT           KEYBD_KEY1
#define KEY_UP              KEYBD_KEY2
#define KEY_DOWN            KEYBD_KEY3
#define KEY_ENTER           KEYBD_KEY4
#define KEY_BACK            KEYBD_KEY5

//--------------------------------------------------------------------

#define LED_PORT        PORTC
#define LED_RED         0x04
#define LED_GRN         0x08
#define LED_BLU         0x10

//------------------------------------------------

extern volatile __tiny byte keybd_state;
extern volatile __tiny byte keybd_async;

extern volatile __tiny byte bklit_cntrl;

//--------------------------------------------------------------------

void bklit_on(void);
void keybd_init(void);
byte keybd_getc(void);

//--------------------------------------------------------------------

#endif /* __keyboard_h__ */

//--------------------------------------------------------------------
