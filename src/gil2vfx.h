#include <stdint.h>

#define PAGE1 0
#define PAGE2 1

#define WND 0 // print() operations: output directly to window
#define BUF 1 //                     output to start of buffer
#define APP 2 //                     append to end of buffer

#define J_LEFT 0 // justification modes for buffered text output
#define J_RIGHT 1
#define J_CENTER 2
#define J_FILL 3

#define NO_MIRROR 0 // mirror equates for draw_bitmap()
#define X_MIRROR 1
#define Y_MIRROR 2
#define XY_MIRROR 3

typedef struct
{
   int32_t window;
   int32_t htab, vtab;
   FONT *font;
   int32_t delay;
   int32_t (*continueFunction)();
   char *txtbuf;
   char *txtpnt;
   int32_t justify;
   uint8_t lookaside[256];
} TEXTWINDOW;

int32_t GIL2VFX_char_width(int32_t ch);
void GIL2VFX_print(int32_t operation, const char *format, ...);
void GIL2VFX_print_buffer(int32_t linenum);

void GIL2VFX_copy_window(uint32_t src, uint32_t dst);
int32_t GIL2VFX_assign_window(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void GIL2VFX_release_window(uint32_t wnd);
void GIL2VFX_wipe_window(int32_t wnd, int32_t color);
void GIL2VFX_refresh_window(uint32_t source, uint32_t target);

int32_t GIL2VFX_assign_subwindow(uint32_t wnd, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void GIL2VFX_release_subwindow(uint32_t wnd);

int32_t GIL2VFX_get_x1(uint32_t wnd);
int32_t GIL2VFX_get_y1(uint32_t wnd);
int32_t GIL2VFX_get_x2(uint32_t wnd);
int32_t GIL2VFX_get_y2(uint32_t wnd);
void GIL2VFX_set_x1(uint32_t wnd, int32_t val);
void GIL2VFX_set_y1(uint32_t wnd, int32_t val);
void GIL2VFX_set_x2(uint32_t wnd, int32_t val);
void GIL2VFX_set_y2(uint32_t wnd, int32_t val);

void GIL2VFX_init();
void GIL2VFX_shutdown_driver();

int32_t GIL2VFX_get_bitmap_width(void *shape_table, int32_t shape_num);
int32_t GIL2VFX_get_bitmap_height(void *shape_table, int32_t shape_num);
int32_t GIL2VFX_visible_bitmap_rect(int32_t x1, int32_t y1, int32_t mirror, uint8_t *shapes, int32_t shape_num, int16_t *bounds);
void GIL2VFX_draw_bitmap(int32_t wnd, int32_t x, int32_t y, int32_t mirror, int32_t scale, uint8_t *fade_table, uint8_t *shapes, int32_t shape_num);

void GIL2VFX_draw_dot(int32_t wnd, int32_t x, int32_t y, int32_t color);
int32_t GIL2VFX_read_dot(int32_t wnd, int32_t x, int32_t y);
void GIL2VFX_draw_line(int32_t wnd, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color);
void GIL2VFX_draw_rect(int32_t wnd, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color);
void GIL2VFX_fill_rect(int32_t wnd, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color);
void GIL2VFX_hash_rect(int32_t wnd, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color);

void GIL2VFX_light_fade(int32_t src_wnd, int32_t color);
void GIL2VFX_color_fade(int32_t src_wnd, int32_t dst_wnd);
void GIL2VFX_pixel_fade(int32_t src_wnd, int32_t dest_wnd, int32_t intervals);

void GIL2VFX_select_text_window(TEXTWINDOW *tw);
int32_t GIL2VFX_char_width(int32_t ch);
void GIL2VFX_home(void);
void GIL2VFX_remap_font_color(int32_t current, int32_t new);
int32_t GIL2VFX_test_overlap(int32_t wnd, int32_t x1, int32_t y1, uint8_t *shapes, int32_t shape_num);
void GIL2VFX_print(int32_t operation, const char *format, ...);
void GIL2VFX_scroll_window(int32_t wnd, int32_t dx, int32_t dy, int32_t flags, int32_t background);
void GIL2VFX_print_buffer(int32_t linenum);
void GIL2VFX_cout(int32_t c);
