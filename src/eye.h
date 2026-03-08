// ����������������������������������������������������������������������������
// ��                                                                        ��
// ��  TEST.H                                                                ��
// ��                                                                        ��
// ��  Sample AESOP code resource/attribute header file                      ��
// ��                                                                        ��
// ��  Project: Extensible State-Object Processor (AESOP/16)                 ��
// ��   Author: John Miles                                                   ��
// ��                                                                        ��
// ��  C source compatible with IBM PC ANSI C/C++ implementations            ��
// ��  AESOP source compatible with AESOP/16 v1.0                            ��
// ��                                                                        ��
// ����������������������������������������������������������������������������
// ��                                                                        ��
// ��  Copyright (C) 1992 Miles Design, Inc.                                 ��
// ��                                                                        ��
// ��  Miles Design, Inc.                                                    ��
// ��  10926 Jollyville #308                                                 ��
// ��  Austin, TX 78759                                                      ��
// ��  (512) 345-2642 / BBS (512) 454-9990 / FAX (512) 338-9630              ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

#include <stdint.h> // Tom: added

#ifdef __AESOP__

// Tom: TODO what is this for
#define PROCDEF

#else

void load_string(int32_t argcnt, int8_t *array, uint32_t string);
void load_resource(int32_t argcnt, int8_t *array, uint32_t resource);
void copy_string(int32_t argcnt, int8_t *src, int8_t *dest);
void string_force_lower(int32_t argcnt, int8_t *dest);
void string_force_upper(int32_t argcnt, int8_t *dest);
uint32_t string_len(int32_t argcnt, int8_t *string);
uint32_t string_compare(int32_t argcnt, int8_t *str1, int8_t *str2);
void beep(void);
int32_t strval(int32_t argcnt, int8_t *string);
int32_t envval(int32_t argcnt, int8_t *name);
void pokemem(int32_t argcnt, int32_t *addr, int32_t data);
int32_t peekmem(int32_t argcnt, int32_t *addr);
uint32_t rnd(int32_t argcnt, uint32_t low, uint32_t high);
uint32_t dice(int32_t argcnt, uint32_t ndice, uint32_t nsides, uint32_t bonus);
uint32_t absv(int32_t argcnt, int32_t val);
int32_t minv(int32_t argcnt, int32_t val1, int32_t val2);
int32_t maxv(int32_t argcnt, int32_t val1, int32_t val2);
void diagnose(int32_t argcnt, uint32_t dtype, uint32_t parm);
uint32_t heapfree(void);

void notify(int32_t argcnt, uint32_t index, uint32_t message, int32_t event, int32_t parameter);
void cancel(int32_t argcnt, uint32_t index, uint32_t message, int32_t event, int32_t parameter);
void drain_event_queue(void);
void post_event(int32_t argcnt, uint32_t owner, int32_t event, int32_t parameter);
void send_event(int32_t argcnt, uint32_t owner, int32_t event, int32_t parameter);
uint32_t peek_event(void);
void dispatch_event(void);
void flush_event_queue(int32_t argcnt, int32_t owner, int32_t event, int32_t parameter);
void flush_input_events(void);

void init_interface(void);
void shutdown_interface(void);
void set_mouse_pointer(int32_t argcnt, uint32_t table, uint32_t number, int32_t hot_X, int32_t hot_Y, uint32_t scale, uint32_t fade_table, uint32_t fade_level);
void set_wait_pointer(int32_t argcnt, uint32_t number, int32_t hot_X, int32_t hot_Y);
void standby_cursor(void);
void resume_cursor(void);
void show_mouse(void);
void hide_mouse(void);
uint32_t mouse_XY(void);
uint32_t mouse_in_window(int32_t argcnt, uint32_t wnd);
void lock_mouse(void);
void unlock_mouse(void);
void getkey(void);

void init_graphics(void);
void draw_dot(int32_t argcnt, uint32_t page, uint32_t x, uint32_t y, uint32_t color);
void draw_line(int32_t argcnt, uint32_t page, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);
void line_to(int32_t argcnt, uint32_t x, uint32_t y, uint32_t color, ...);
void draw_rectangle(int32_t argcnt, uint32_t wndnum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
void fill_rectangle(int32_t argcnt, uint32_t wndnum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
void hash_rectangle(int32_t argcnt, uint32_t wndnum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
uint32_t get_bitmap_height(int32_t argcnt, uint32_t table, uint32_t number);
void draw_bitmap(int32_t argcnt, uint32_t page, uint32_t table, uint32_t number, int32_t x, int32_t y, uint32_t scale, uint32_t flip, uint32_t fade_table, uint32_t fade_level);
uint32_t visible_bitmap_rect(int32_t argcnt, int32_t x, int32_t y, uint32_t flip, uint32_t table, uint32_t number, int16_t *array);
void set_palette(int32_t argcnt, uint32_t region, uint32_t resource);
void refresh_window(int32_t argcnt, uint32_t src, uint32_t target);
void wipe_window(int32_t argcnt, uint32_t window, uint32_t color);
void shutdown_graphics(void);
void wait_vertical_retrace(void);
uint32_t read_palette(int32_t argcnt, uint32_t regnum);
void write_palette(int32_t argcnt, uint32_t regnum, uint32_t value);
void pixel_fade(int32_t argcnt, uint32_t src_wnd, uint32_t dest_wnd, uint32_t intervals);
void color_fade(int32_t argcnt, uint32_t src_wnd, uint32_t dest_wnd);
void light_fade(int32_t argcnt, uint32_t src_wnd, uint32_t color);

uint32_t assign_window(int32_t argcnt, uint32_t owner, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2);
uint32_t assign_subwindow(int32_t argcnt, uint32_t owner, uint32_t parent, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2);
void release_window(int32_t argcnt, uint32_t window);
uint32_t get_x1(int32_t argcnt, uint32_t window);
uint32_t get_x2(int32_t argcnt, uint32_t window);
uint32_t get_y1(int32_t argcnt, uint32_t window);
uint32_t get_y2(int32_t argcnt, uint32_t window);
void set_x1(int32_t argcnt, uint32_t window, uint32_t x1);
void set_x2(int32_t argcnt, uint32_t window, uint32_t x2);
void set_y1(int32_t argcnt, uint32_t window, uint32_t y1);
void set_y2(int32_t argcnt, uint32_t window, uint32_t y2);

void text_window(int32_t argcnt, uint32_t wndnum, uint32_t wnd);
void text_style(int32_t argcnt, uint32_t wndnum, uint32_t font, uint32_t justify);
void text_xy(int32_t argcnt, uint32_t wndnum, uint32_t htab, uint32_t vtab);
void text_color(int32_t argcnt, uint32_t wndnum, uint32_t current, uint32_t new);
void text_refresh_window(int32_t argcnt, uint32_t wndnum, int32_t wnd);

int32_t get_text_x(int32_t argcnt, uint32_t wndnum);
int32_t get_text_y(int32_t argcnt, uint32_t wndnum);

void home(int32_t argcnt, uint32_t wndnum);

void print(int32_t argcnt, uint32_t wndnum, uint32_t format, ...);
void sprint(int32_t argcnt, uint32_t wndnum, int8_t *format, ...);
void dprint(int32_t argcnt, int8_t *format, ...);
void aprint(int32_t argcnt, int8_t *format, ...);
void crout(int32_t argcnt, uint32_t wndnum);
uint32_t char_width(int32_t argcnt, uint32_t wndnum, uint32_t ch);
uint32_t font_height(int32_t argcnt, uint32_t wndnum);

void solid_bar_graph(int32_t argcnt, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t lb_border, uint32_t tr_border, uint32_t bkgnd, uint32_t grn, uint32_t yel, uint32_t red, int32_t val, int32_t min, int32_t crit, int32_t max);

void init_sound(int32_t argcnt, uint32_t errprompt);
void shutdown_sound(void);
void load_sound_block(int32_t argcnt, uint32_t first_block, uint32_t last_block, uint32_t *array);
void sound_effect(int32_t argcnt, uint32_t index);
void play_sequence(int32_t argcnt, uint32_t LA_version, uint32_t AD_version, uint32_t PC_version);
void load_music(void);
void unload_music(void);
void set_sound_status(int32_t argcnt, uint32_t status);

int32_t create_object(int32_t argcnt, uint32_t name);
int32_t create_program(int32_t argcnt, int32_t index, uint32_t name);
int32_t destroy_object(int32_t argcnt, int32_t index);
void thrash_cache(void);
uint32_t flush_cache(int32_t argcnt, uint32_t goal);

int32_t step_X(int32_t argcnt, uint32_t x, uint32_t fdir, uint32_t mtype, uint32_t distance);
int32_t step_Y(int32_t argcnt, uint32_t y, uint32_t fdir, uint32_t mtype, uint32_t distance);
uint32_t step_FDIR(int32_t argcnt, uint32_t fdir, uint32_t mtype);

int32_t step_square_X(int32_t argcnt, uint32_t x, uint32_t r, uint32_t dir);
int32_t step_square_Y(int32_t argcnt, uint32_t y, uint32_t r, uint32_t dir);
int32_t step_region(int32_t argcnt, uint32_t r, uint32_t dir);

uint32_t distance(int32_t argcnt, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2);
uint32_t seek_direction(int32_t argcnt, uint32_t obj_x, uint32_t obj_y, uint32_t dest_x, uint32_t dest_y);

uint32_t spell_request(int32_t argcnt, int8_t *stat, int8_t *cnt, uint32_t typ, uint32_t num);
uint32_t spell_list(int32_t argcnt, int8_t *cnt, uint32_t typ, uint32_t lvl, int8_t *list, uint32_t max);
void magic_field(int32_t argcnt, uint32_t p, uint32_t redfield, uint32_t yelfield, int32_t sparkle);
void do_dots(int32_t argcnt, int32_t view, int32_t scrn, int32_t exp_x, int32_t exp_y, int32_t scale, int32_t power, int32_t dots, int32_t life, int32_t upval, int8_t *colors);
void do_ice(int32_t argcnt, int32_t view, int32_t scrn, int32_t dots, int32_t mag, int32_t grav, int32_t life, int32_t colors);

void read_save_directory(void);
int8_t *savegame_title(int32_t argcnt, uint32_t num);
void write_save_directory(void);

uint32_t save_game(int32_t argcnt, uint32_t slotnum, uint32_t lvlnum);
void suspend_game(int32_t argcnt, uint32_t cur_lvl);
void resume_items(int32_t argcnt, uint32_t first, uint32_t last, uint32_t restoring);
void resume_level(int32_t argcnt, uint32_t cur_lvl);
void change_level(int32_t argcnt, uint32_t old_lvl, uint32_t new_lvl);
void restore_items(int32_t argcnt, uint32_t slotnum);
void restore_level_objects(int32_t argcnt, uint32_t slotnum, uint32_t lvlnum);
void read_initial_items(void);
void write_initial_tempfiles(void);
void create_initial_binary_files(void);
void launch(int32_t argcnt, int8_t *dirname, int8_t *prgname, int8_t *argn1, int8_t *argn2);

void mono_on(void);
void mono_off(void);
void *open_transfer_file(int32_t argcnt, int8_t *filename);
void close_transfer_file(void);
int32_t player_attrib(int32_t argcnt, uint32_t plrnum, uint32_t offset, uint32_t size);
int32_t item_attrib(int32_t argcnt, uint32_t plrnum, uint32_t invslot, uint32_t attrib);
int32_t arrow_count(int32_t argcnt, uint32_t plrnum);

// typedef void (cdecl *FARPROC)(); // Tom: TODO
// Tom: `maybe typedef void (*FARPROC)(void);`
#define code_resources FARPROC code_resources[] =
#define PROCDEF (FARPROC)

#endif

// Tom: TODO what is all of this below

#ifdef __AESOP__

//�������������������������������������������
//��                                       ��
//�� AESOP resource attribute declarations ��
//��                                       ��
//�������������������������������������������

attrib sequence fixed,precious
attrib sample fixed,precious
attrib string moveable,discardable
attrib source moveable,discardable
attrib document temporary
attrib map temporary
attrib palette moveable,discardable
attrib file moveable,discardable

#endif

//����������������������������������������
//��                                    ��
//�� AESOP/C code resource declarations ��
//��                                    ��
//����������������������������������������

code_resources
{
   //
   // Miscellaneous functions
   //

   PROCDEF load_string,
   PROCDEF load_resource,
   PROCDEF copy_string,
   PROCDEF string_force_lower,
   PROCDEF string_force_upper,
   PROCDEF string_len,
   PROCDEF string_compare,
   PROCDEF strval,
   PROCDEF envval,
   PROCDEF beep,
   PROCDEF pokemem,
   PROCDEF peekmem,
   PROCDEF rnd,
   PROCDEF dice,
   PROCDEF absv,
   PROCDEF minv,
   PROCDEF maxv,
   PROCDEF diagnose,
   PROCDEF heapfree,

   //
   // Event functions
   // 

   PROCDEF notify,
   PROCDEF cancel,
   PROCDEF drain_event_queue,
   PROCDEF post_event,
   PROCDEF send_event,
   PROCDEF peek_event,
   PROCDEF dispatch_event,
   PROCDEF flush_event_queue,
   PROCDEF flush_input_events,

   //
   // Interface functions
   //

   PROCDEF init_interface,
   PROCDEF shutdown_interface,
   PROCDEF set_mouse_pointer,
   PROCDEF set_wait_pointer,
   PROCDEF standby_cursor,
   PROCDEF resume_cursor,
   PROCDEF show_mouse,
   PROCDEF hide_mouse,
   PROCDEF mouse_XY,
   PROCDEF mouse_in_window,
   PROCDEF lock_mouse,
   PROCDEF unlock_mouse,
   PROCDEF getkey,

   //
   // Graphics-related functions
   //

   PROCDEF init_graphics,
   PROCDEF draw_dot,
   PROCDEF draw_line,
   PROCDEF line_to,
   PROCDEF draw_rectangle,
   PROCDEF fill_rectangle,
   PROCDEF hash_rectangle,
   PROCDEF get_bitmap_height,
   PROCDEF draw_bitmap,
   PROCDEF visible_bitmap_rect,
   PROCDEF set_palette,
   PROCDEF refresh_window,
   PROCDEF wipe_window,
   PROCDEF shutdown_graphics,
   PROCDEF wait_vertical_retrace,
   PROCDEF read_palette,
   PROCDEF write_palette,
   PROCDEF pixel_fade,
   PROCDEF color_fade,
   PROCDEF light_fade,

   PROCDEF assign_window,
   PROCDEF assign_subwindow,
   PROCDEF release_window,
   PROCDEF get_x1,
   PROCDEF get_x2,
   PROCDEF get_y1,
   PROCDEF get_y2,
   PROCDEF set_x1,
   PROCDEF set_x2,
   PROCDEF set_y1,
   PROCDEF set_y2,

   PROCDEF text_window,
   PROCDEF text_style,
   PROCDEF text_xy,
   PROCDEF text_color,
   PROCDEF text_refresh_window,
   PROCDEF get_text_x,
   PROCDEF get_text_y,
   PROCDEF home,
   PROCDEF print,
   PROCDEF sprint,
   PROCDEF dprint,
   PROCDEF aprint,
   PROCDEF crout,
   PROCDEF char_width,
   PROCDEF font_height,

   PROCDEF solid_bar_graph,

   PROCDEF mono_on,
   PROCDEF mono_off,

   //
   // Sound-related functions
   //

   PROCDEF init_sound,
   PROCDEF shutdown_sound,
   PROCDEF load_sound_block,
   PROCDEF sound_effect,
   PROCDEF play_sequence,
   PROCDEF load_music,
   PROCDEF unload_music,
   PROCDEF set_sound_status,

   //
   // Eye III object management
   //

   PROCDEF create_object,
   PROCDEF create_program,
   PROCDEF destroy_object,
   PROCDEF flush_cache,
   PROCDEF thrash_cache,

   //
   // Eye III support functions
   //

   PROCDEF step_X,
   PROCDEF step_Y,
   PROCDEF step_FDIR,

   PROCDEF step_square_X,
   PROCDEF step_square_Y,
   PROCDEF step_region,

   PROCDEF distance,
   PROCDEF seek_direction,

   PROCDEF spell_request,
   PROCDEF spell_list,
   PROCDEF magic_field,
   PROCDEF do_dots,
   PROCDEF do_ice,

   PROCDEF read_save_directory,
   PROCDEF savegame_title,
   PROCDEF write_save_directory,

   PROCDEF save_game,
   PROCDEF suspend_game,
   PROCDEF resume_items,
   PROCDEF resume_level,
   PROCDEF change_level,
   PROCDEF restore_items,
   PROCDEF restore_level_objects,
   PROCDEF read_initial_items,
   PROCDEF write_initial_tempfiles,
   PROCDEF create_initial_binary_files,
   PROCDEF launch,

   //
   // Eye II savegame file access
   //

   PROCDEF open_transfer_file,
   PROCDEF close_transfer_file,
   PROCDEF player_attrib,
   PROCDEF item_attrib,
   PROCDEF arrow_count,
};
