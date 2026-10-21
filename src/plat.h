/* plat.h - SMS platform layer used by the game code (replaces the Genesis
 * port's vid/draw2d/input/menu/save/snd headers).
 *
 * Bank 0 (plat0.c, snd.c): string entry points (they copy their string
 *   arguments to RAM first, so callers may pass strings from their own
 *   bank), ROM data accessors, math, timer, sound player.
 * Banked (platform bank): 2-D drawing, HUD, video modes, input, menus,
 *   saves.  Functions taking strings there must only get RAM strings.
 */
#ifndef PLAT_H
#define PLAT_H
#include "hw.h"

#ifndef B
#define B __banked
#endif

/* ----------------------------------------------------------- bank 0 */
u32  get_timecount(void);
void set_timecount(u32 t);

extern char plat_str[320];             /* string argument copies */
extern char plat_str2[128];

void Quit(const char *error);
const char *LevelName(s16 n);           /* RAM copy, valid until the next call */
const char *LevelText(s16 map, s16 n);  /* RAM copy */
u16  text_copy(u16 id, char *dst, u16 max);
u16  help_read(u16 page, u16 pos, char *dst, u16 n); /* help page bytes, 0 terminated */
void rom_read(u8 bank, u16 addr, void *dst, u16 len);  /* data bank via slot 1 */
u16  random16(void);
u16  udiv32_16(u32 n, u16 d);           /* n < d << 16 */
void intToStr(s16 v, char *s, u8 minlen);
void random_seed(u16 s);

void US_Print(const char *s);
void US_CPrint(const char *s);
void US_CPrintLine(const char *s);
void US_PrintCentered(const char *s);
char DisplayMsg(const char *text, const char *choices);
char DisplaySMsg(const char *text, const char *choices);
char MenuChoice(const char *prompt, const char *choices);
s16  MenuWindow(const char *title, u16 count, const char * const *items, s16 sel);

/* sound (snd.c, bank 0) */
typedef enum { sdm_Off, sdm_PC, sdm_AdLib } SDMode;
extern SDMode SoundMode;
void SD_Startup(void);
void SD_SetSoundMode(SDMode mode);
void SD_PlaySound(u16 sound);
void SD_StopSound(void);
void SD_WaitSoundDone(void);
u16  SD_SoundPlaying(void);
void snd_tick(void);
void snd_beep(void);

/* ------------------------------------------------------------ banked */
/* drawing surfaces: tile(tx,ty) = tile0 + tx*th + ty */
typedef struct
{
    u16 tile0;
    u8  th, tw;
    u8  view;           /* the 3-D view: cells get their own tile + NT entry */
} Surf;

extern Surf viewsurf, msgsurf, smsgsurf, fullsurf, facesurf, radarsurf;
extern Surf *cursurf;
extern s16  px, py;
extern u8   fontcolor;
extern u8   fontxor;
extern s16  PrintX, PrintY, WindowX, WindowY, WindowW, WindowH;
extern s16  PPT_LeftEdge, PPT_RightEdge;
extern u8   screenfaded;

void VW_Bar(s16 x, s16 y, s16 w, s16 h, u8 color) B;
void VW_Plot(s16 x, s16 y, u8 color) B;
void VW_ClearSurface(u8 color) B;
void VW_DrawPropStringR(const char *s) B;      /* RAM string */
u16  VW_MeasurePropStringR(const char *s) B;   /* RAM string, width */
void US_PrintR(const char *s) B;
void US_CPrintR(const char *s) B;
void US_CPrintLineR(const char *s) B;
void US_PrintCenteredR(const char *s) B;
void US_PrintUnsigned(u32 n) B;
void US_ClearWindow(void) B;
void US_DrawWindow(u16 x, u16 y, u16 w, u16 h) B;
void US_CenterWindow(u16 w, u16 h) B;
void CenterWindow(u16 w, u16 h) B;
char DisplayMsgR(const char *text, const char *choices) B;
char DisplaySMsgR(const char *text, const char *choices) B;
char MenuChoiceR(const char *prompt, const char *choices) B;
s16  MenuWindowR(const char *title, u16 count, const char * const *items, s16 sel) B;

/* HUD (status area) */
void hud_load(void) B;                  /* tiles + nametable of the play screen */
void hud_num(u8 x, u8 y, s16 value, u8 digits) B;
void hud_char(u8 x, u8 y, u8 ch) B;
void hud_face(u8 n) B;                  /* 0..3 FACE1..4, 4 FACE5 (dead) */
void hud_scroll(u8 n, u8 owned) B;
void hud_gems(const s16 *gems) B;
void hud_radar(const s8 (*blips)[3], s16 nx, s16 ny) B;
void hud_freeze(s16 percent) B;         /* -1: hide */
void hud_freeze_hide(void) B;
extern u8 hud_freeze_on;
void hud_invalidate(void) B;

/* video */
void vid_init_game(void) B;             /* game screen (HUD + view) from scratch */
void vid_fullscreen(u8 on) B;           /* 224x128 text surface (help, menus) */
void vid_show_screen_rom(u8 bank, u16 addr) B;  /* screen blob, display off */
void vid_finale(void) B;                /* FINALEPIC over the view area */
void VW_WaitVBL(u16 n) B;
void VW_FadeOut(void) B;
void VW_FadeIn(void) B;
void VW_ColorBorder(s16 color) B;
void vid_set_palette_black(void) B;
void vid_set_palette_normal(void) B;
void fizzle_begin(void) B;              /* blank the view, renderer holds the NT */
void FizzleFade(void) B;               /* reveal what render_draw drew */

/* input */
typedef struct
{
    u8 fire;            /* 1 held */
    u8 strafe;          /* 2 held with LEFT/RIGHT */
    u8 quickturn;
    u8 zapper;          /* one-shot */
    u8 nuke;
    u8 potion;
    u8 options;
    u8 scrolls;
    u8 debug;
} GameButtons;
extern GameButtons gbuttons;
extern u8 pad_now, pad_last;
#define PAD_OK      PAD_1
#define PAD_CANCEL  PAD_2
void IN_ReadControl(void *info) B;      /* ControlInfo* */
void IN_PollGameButtons(void) B;
void IN_ClearKeysDown(void) B;
void IN_Ack(void) B;
u8   IN_WaitPress(void) B;
u8   IN_PollPress(void) B;

/* menus */
s16  MenuScroll(void) B;
void HelpScreens(void) B;
void PreFullDisplay(void) B;
void PostFullDisplay(u8 draw_view) B;
void GE_SaveGame(void) B;
u8   GE_LoadGame(void) B;
void ShowScroll(u8 n) B;
void IntroProgram(void) B;
void EndScreens(void) B;
void gfx_init(void) B;
void gfx_text(s16 x, s16 y, const char *s, u8 c) B;   /* RAM string */

/* saves (cartridge RAM bank 1) */
#define SAVE_SLOTS 3
void save_init(void) B;
u8   save_slot_info(u8 slot, char *desc) B; /* desc: 32 bytes RAM */

#endif
