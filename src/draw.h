/* draw.h - small GDI toolkit shared by taskbar, start panel, title bars, settings. */
#ifndef DRAW_H
#define DRAW_H
#include <windows.h>
#include "themes.h"

enum { G_MIN, G_MAX, G_RESTORE, G_CLOSE, G_PIN, G_SHADE, G_POWER, G_GEAR, G_RUN,
       G_BATTERY, G_WIFI, G_BT, G_TASKMGR, G_PLUS, G_CHEV_UP, G_CHEV_DOWN, G_DESKTOP, G_GRID };

#define ICON_PRESETS 16
extern const wchar_t *g_presetNames[ICON_PRESETS];

void Draw_Fill(HDC dc, const RECT *r, COLORREF c);
void Draw_Round(HDC dc, const RECT *r, COLORREF fill, COLORREF line, int radius);
void Draw_Bevel(HDC dc, const RECT *r, BOOL raised);
void Draw_Gradient(HDC dc, const RECT *r, COLORREF a, COLORREF b, BOOL vertical);
void Draw_Alpha(HDC dc, const RECT *r, COLORREF c, int alpha);          /* translucent fill */
void Draw_Text(HDC dc, const RECT *r, const wchar_t *s, COLORREF c, HFONT f, UINT fmt);
/* panel background with the selected effect (glass / blur / 3D / boxed / liquid) */
void Draw_Surface(HDC dc, const RECT *r, const Palette *p, COLORREF base, BOOL applyFx);
void Draw_StartButton(HDC dc, const RECT *r, const Palette *p, BOOL hot, BOOL down);
void Draw_Glyph(HDC dc, const RECT *r, int glyph, COLORREF c, int preset);
void Draw_Icon(HDC dc, int x, int y, int size, HICON ic, const Palette *p);
void Draw_Shadow(HDC dc, const RECT *r, int spread);

HICON Icon_ForPath(const wchar_t *path, BOOL large);       /* cached, do not destroy */
HICON Icon_ForWindow(HWND w);                              /* may return default app icon */
COLORREF Icon_Dominant(HICON ic, COLORREF fallback);       /* for underline colour adapt */

/* pictures: PNG/JPG/GIF/BMP via GDI+ (loaded at run time), cached by path */
HBITMAP Img_Load(const wchar_t *path);
void    Img_Forget(void);
void    Img_Draw(HDC dc, const RECT *r, HBITMAP bmp, int fit);   /* 0 crop 1 fit 2 stretch 3 centre 4 tile */
void    Img_DrawCircle(HDC dc, const RECT *r, HBITMAP bmp, COLORREF fallback);

/* make window translucent / rounded helpers */
void Win_SetAlpha(HWND w, int alpha);
void Win_SetRound(HWND w, int radius);

#endif
