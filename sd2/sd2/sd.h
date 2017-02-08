//------------------------------------------------

#ifndef __sd_h__
#define __sd_h__

//------------------------------------------------

//#include <iom16.h>
//#include <inavr.h>
#include "common.h"
#include "clock.h"
#include "types.h"
#include "spi.h"
#include "lcd.h"

//------------------------------------------------

#define SD_CS_PORT      PORTC
#define SD_CS           0x20

#define SD_CS_INVERT

//------------------------------------------------

#define SD_RESET                  0
#define SD_START_INITIALIZATION   1
#define SD_SEND_CSD               9
#define SD_SEND_CID              10
#define SD_STOP_TRANSMISSION     12
#define SD_SEND_STATUS           13
#define SD_SET_BLOCK_SIZE        16
#define SD_READ_SINGLE_BLOCK     17
#define SD_READ_MULTIPLE_BLOCKS  18
#define SD_WRITE_SINGLE_BLOCK    24
#define SD_WRITE_MULTIPLE_BLOCKS 25
#define SD_PROGRAM_CSD           27
#define SD_SET_WRITE_PROT        28
#define SD_CLR_WRITE_PROT        29
#define SD_SEND_WRITE_PROT       30
#define SD_TAG_SECTOR_START      32
#define SD_TAG_SECTOR_END        33 
#define SD_UNTAG_SECTOR          34
#define SD_TAG_ERASE_GROUP_START 35
#define SD_TAG_ERASE_GROUP_END   36
#define SD_UNTAG_ERASE_GROUP     37
#define SD_ERASE                 38
#define SD_LOCK_UNLOCK           42
#define SD_READ_OCR              58 
#define SD_CRC_ON_OFF            59 

//------------------------------------------------

#define SD_CS_HIGH               SD_CS_PORT |= SD_CS
#define SD_CS_LOW                SD_CS_PORT &= ~SD_CS

#ifndef SD_CS_INVERT
 #define SD_CS_ASSERT()          SD_CS_LOW
 #define SD_CS_DEASSERT()        SD_CS_HIGH
#else
 #define SD_CS_ASSERT()          SD_CS_HIGH
 #define SD_CS_DEASSERT()        SD_CS_LOW
#endif

//------------------------------------------------

byte sd_init(void);
dword sd_get_sector_count(void);

byte sd_prep_read_sector(dword addr);
void sd_end_read_sector(void);
byte sd_prep_write_sector(dword addr);
byte sd_end_write_sector(void);
byte sd_read_block(byte *buf, dword addr, byte n, byte verify);
byte sd_read(byte *buf, dword addr, byte n);

//------------------------------------------------

#endif /* __sd_h__ */

//------------------------------------------------
