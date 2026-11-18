/* themes.h - 22 OS looks, each with a MODERN variant and a CLASSIC "OG" variant. */
#ifndef THEMES_H
#define THEMES_H
#include <windows.h>
#ifndef CLR_NONE
#define CLR_NONE 0xFFFFFFFFL
#endif

enum { SHAPE_LOGO = 0, SHAPE_TEXT98, SHAPE_TEXTXP, SHAPE_ORB, SHAPE_CIRCLE, SHAPE_SQUARE };
enum { FX_NONE = 0, FX_GLASS, FX_IOS_BLUR, FX_3D, FX_BOXED, FX_LIQUID };
enum { FAMILY_WINDOWS = 0, FAMILY_LINUX, FAMILY_APPLE };

typedef struct {
    const wchar_t *name;
    COLORREF mBg, mAccent, mStart;      /* modern  */
    COLORREF cBg, cAccent, cStart;      /* classic (original look) */
    int  cHeight;                       /* original taskbar height */
    int  centered;                      /* icons centred in the bar (Win11 / dock style) */
    int  shape;                         /* start button renderer */
    int  bevel;                         /* classic 3D bevel */
    int  radius;                        /* modern corner radius */
    int  glass;                         /* original used glass */
    int  family;
    int  titleStyle;                    /* index of matching title-bar style */
} ThemeDef;

/* resolved colours / metrics actually used for drawing (all auto-adapted) */
typedef struct {
    COLORREF bg, fg, fgDim, accent, startBg, startFg;
    COLORREF panel, panelFg, tile, hover, border, shadow;
    int height, radius, bevel, centered, shape, glass, classic, effect;
} Palette;

#define THEME_COUNT 23
extern const ThemeDef g_themes[THEME_COUNT];
extern const wchar_t *g_modeNames[7];
extern const wchar_t *g_effectNames[6];

void     Theme_Resolve(Palette *p);          /* from g_cfg */
COLORREF Col_Blend(COLORREF a, COLORREF b, int pctB);
int      Col_Lum(COLORREF c);                /* 0..255 */
COLORREF Col_Text(COLORREF bg);              /* readable text colour for bg */
COLORREF Col_Tile(int i);                    /* stable per-tile colour */
HFONT    Theme_Font(int pt, BOOL bold);      /* caller does not delete: cached */
int      UiScale(int px);                    /* px * p_uiscale / 100 */

#endif
