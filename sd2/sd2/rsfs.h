//------------------------------------------------

#ifndef __rsfs_h__
#define __rsfs_h__

//------------------------------------------------

//#include <iom16.h>
//#include <inavr.h>
#include "common.h"
#include "clock.h"
#include "types.h"
#include "lcd.h"
#include "sd.h"
#include "terminal.h"
#include "keyboard.h"
#include "menu.h"

//------------------------------------------------

#define FS_ROOT     0
#define FS_FIRST    1
#define FS_CUR      2
#define FS_NEXT     3
#define FS_PREV     4

#define FS_FILE     1
#define FS_FOLDER   2

//------------------------------------------------

typedef struct __direntry
{
    dword entry;
    char  name [33];
    dword start;
    dword size;
    byte  isdir;
} direntry;

//------------------------------------------------

extern dword fs_base_ptr;
extern dword fs_read_ptr;
extern dword fs_file_ptr;
extern dword fs_file_siz;

//------------------------------------------------

byte fs_seek(dword offset, byte curor);
byte fs_read(byte *buf, byte count);
byte fs_find(direntry *p, byte mode);
byte fs_browse(direntry *p, byte mode);
void fs_open(direntry *p);
void fs_open_entry(dword ptr);

//------------------------------------------------

#endif /* __rsfs_h__ */

//------------------------------------------------
