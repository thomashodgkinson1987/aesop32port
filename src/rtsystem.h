//
// AESOP interpreter system services
//

#ifndef RTSYSTEM_H
#define RTSYSTEM_H

#include <stdint.h>

#define TF_BUFSIZE 4096 // size of buffer allocated by file functions

typedef struct
{
   uint32_t hbuf;
   int8_t *buffer;
   int16_t p;
   int16_t file;
   int16_t mode;
   int32_t len;
   int32_t pos;
} TF_class;

#define TF_WRITE 0
#define TF_READ 1

#define isnum(x) (((x) == '-') || (((x) >= '0') && ((x) <= '9')))

//
// Universal disk error codes
//

#define NO_ERROR 0
#define IO_ERROR 1
#define OUT_OF_MEMORY 2
#define FILE_NOT_FOUND 3
#define CANT_WRITE_FILE 4
#define CANT_READ_FILE 5
#define DISK_FULL 6

//
// General file management
//

int16_t copy_file(int8_t *src_filename, int8_t *dest_filename);
int16_t delete_file(int8_t *filename);
uint32_t file_time(int8_t *filename);

//
// Text file management
//

TF_class *TF_construct(int8_t *filename, int16_t oflag);
int16_t TF_destroy(TF_class *TF);
int16_t TF_wchar(TF_class *TF, int8_t ch);
int8_t TF_rchar(TF_class *TF);
int16_t TF_readln(TF_class *TF, int8_t *buffer, int16_t maxlen);
int16_t TF_writeln(TF_class *TF, int8_t *buffer);

//
// Binary file management
//

int32_t file_size(int8_t *filename);
int8_t *read_file(int8_t *filename, void *dest);
int16_t write_file(int8_t *filename, void *buf, uint32_t len);
int16_t append_file(int8_t *filename, void *buf, uint32_t len);

//
// Memory heap management
//

void mem_init(void);
void mem_shutdown(void);
uint32_t mem_avail(void);
void *mem_alloc(uint32_t bytes);
int8_t *str_alloc(int8_t *string);
void mem_free(void *ptr);
uint32_t mem_headroom(void);

//
// Misc. routines
//

int32_t ascnum(int8_t *string);
void opcode_fault(void *PC, void *stk);
void abend(char *msg, ...);
void curpos(int16_t *x, int16_t *y);
void locate(int16_t x, int16_t y);

#endif
