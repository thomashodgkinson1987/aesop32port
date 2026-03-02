//
// 386FX file & DLL loader functions/equates
//

#ifndef DLL_H
#define DLL_H

//
// MetaWare support
//

#ifdef __HIGHC__
#define cdecl _CC(_REVERSE_PARMS | _NEAR_CALL) // Tom: TODO
#pragma Global_aliasing_convention("_%r");
#endif

#ifndef FILE_ERRS
#define FILE_ERRS

#define NO_ERROR 0
#define IO_ERROR 1
#define OUT_OF_MEMORY 2
#define FILE_NOT_FOUND 3
#define CANT_WRITE_FILE 4
#define CANT_READ_FILE 5
#define DISK_FULL 6

#endif

#ifndef TYPEDEFS
#define TYPEDEFS

// Tom: TODO

typedef unsigned char UBYTE;
typedef unsigned short UWORD;
typedef unsigned long ULONG;
typedef char BYTE;
typedef short WORD;
typedef long LONG;

#endif

//
// DLL loader flags & functions
//

#define DLLSRC_FILE 0  // *source is filename string
#define DLLSRC_MEM 1   // *source is pointer to DLL image in memory
#define DLLMEM_USER 2  // *dll -> output memory block alloc'd by user
#define DLLMEM_ALLOC 4 // *dll = don't care; allocate & return output mem

ULONG DLL_size(void *source, ULONG flags);
void *DLL_load(void *source, ULONG flags, void *dll);

//
// File functions
//

LONG FILE_error(void);
LONG FILE_size(BYTE *filename);
void *FILE_read(BYTE *filename, void *dest);
LONG FILE_write(BYTE *filename, void *buf, ULONG len);
LONG FILE_append(BYTE *filename, void *buf, ULONG len);

//
// MetaWare support
//

#ifdef __HIGHC__
#pragma Global_aliasing_convention();
#endif

#endif
