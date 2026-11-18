#include "themes.h"
#include "settings.h"

#define C(r,g,b) RGB(r,g,b)

/* name, modern{bg,accent,start}, classic{bg,accent,start}, cHeight, centered, shape,
 * bevel, radius, glass, family, titleStyle */
const ThemeDef g_themes[THEME_COUNT] = {
 { L"None",           C(32,32,32),  C(0,120,215),  C(0,120,215),   C(32,32,32),   C(0,120,215),  C(0,120,215),  40,0,SHAPE_LOGO,   0, 0,0,FAMILY_WINDOWS, 5 },
 { L"Windows 98",     C(44,44,60),  C(0,0,128),    C(192,192,192), C(192,192,192), C(0,0,128),    C(192,192,192),28,0,SHAPE_TEXT98, 1, 0,0,FAMILY_WINDOWS, 1 },
 { L"Windows XP",     C(30,60,130), C(60,160,60),  C(60,160,60),   C(36,94,220),  C(49,106,197), C(58,142,58),  30,0,SHAPE_TEXTXP, 0, 6,0,FAMILY_WINDOWS, 2 },
 { L"Windows 7",      C(30,30,36),  C(70,150,230), C(70,150,230),  C(28,44,66),   C(90,160,230), C(40,110,190), 40,0,SHAPE_ORB,    0, 0,1,FAMILY_WINDOWS, 3 },
 { L"Windows 8",      C(22,22,30),  C(0,120,215),  C(0,120,215),   C(28,28,40),   C(0,150,136),  C(28,28,40),   40,0,SHAPE_LOGO,   0, 0,0,FAMILY_WINDOWS, 4 },
 { L"Windows 10",     C(24,24,24),  C(0,120,215),  C(0,120,215),   C(22,22,22),   C(0,120,215),  C(22,22,22),   40,0,SHAPE_LOGO,   0, 0,0,FAMILY_WINDOWS, 5 },
 { L"Windows 11",     C(32,32,32),  C(0,103,192),  C(0,103,192),   C(32,32,32),   C(0,103,192),  C(32,32,32),   48,1,SHAPE_LOGO,   0, 8,0,FAMILY_WINDOWS, 6 },
 { L"macOS",          C(40,40,48),  C(10,132,255), C(10,132,255),  C(60,60,70),   C(10,132,255), C(60,60,70),   58,1,SHAPE_CIRCLE, 0,14,1,FAMILY_APPLE,   7 },
 { L"Ubuntu",         C(38,38,38),  C(233,84,32),  C(233,84,32),   C(51,51,51),   C(233,84,32),  C(233,84,32),  40,0,SHAPE_CIRCLE, 0, 6,0,FAMILY_LINUX,   8 },
 { L"Arch Linux",     C(20,20,24),  C(23,147,209), C(23,147,209),  C(23,23,23),   C(23,147,209), C(23,147,209), 30,0,SHAPE_SQUARE, 0, 0,0,FAMILY_LINUX,   9 },
 { L"Linux Mint",     C(40,40,40),  C(141,180,76), C(141,180,76),  C(47,47,47),   C(141,180,76), C(141,180,76), 36,0,SHAPE_SQUARE, 0, 4,0,FAMILY_LINUX,  10 },
 { L"XFCE",           C(50,50,54),  C(40,120,200), C(40,120,200),  C(58,58,58),   C(40,120,200), C(58,58,58),   30,0,SHAPE_SQUARE, 0, 0,0,FAMILY_LINUX,  11 },
 { L"Cinnamon",       C(30,30,30),  C(141,180,76), C(141,180,76),  C(30,30,30),   C(141,180,76), C(141,180,76), 36,0,SHAPE_CIRCLE, 0, 4,0,FAMILY_LINUX,  12 },
 { L"KDE Plasma",     C(35,38,41),  C(61,174,233), C(61,174,233),  C(35,38,41),   C(61,174,233), C(61,174,233), 44,0,SHAPE_SQUARE, 0, 6,0,FAMILY_LINUX,  13 },
 { L"Zorin OS",       C(31,31,31),  C(21,167,241), C(21,167,241),  C(31,31,31),   C(21,167,241), C(21,167,241), 42,0,SHAPE_SQUARE, 0, 8,0,FAMILY_LINUX,  14 },
 { L"flowOS",         C(24,24,32),  C(122,90,248), C(122,90,248),  C(24,24,32),   C(122,90,248), C(122,90,248), 50,1,SHAPE_CIRCLE, 0,12,1,FAMILY_LINUX,  15 },
 { L"elementary OS",  C(35,40,45),  C(62,150,224), C(62,150,224),  C(35,40,45),   C(62,150,224), C(62,150,224), 52,1,SHAPE_CIRCLE, 0,10,0,FAMILY_LINUX,  16 },
 { L"Pop!_OS",        C(48,46,46),  C(72,185,199), C(72,185,199),  C(51,50,50),   C(72,185,199), C(72,185,199), 36,0,SHAPE_SQUARE, 0, 4,0,FAMILY_LINUX,  17 },
 { L"Deepin",         C(30,30,34),  C(0,130,250),  C(0,130,250),   C(30,30,34),   C(0,130,250),  C(0,130,250),  50,1,SHAPE_CIRCLE, 0,10,1,FAMILY_LINUX,  18 },
 { L"Fedora GNOME",   C(30,30,30),  C(60,110,180), C(60,110,180),  C(30,30,30),   C(60,110,180), C(60,110,180), 32,0,SHAPE_CIRCLE, 0, 0,0,FAMILY_LINUX,  19 },
 { L"ChromeOS",       C(32,33,36),  C(138,180,248),C(138,180,248), C(32,33,36),   C(138,180,248),C(138,180,248),48,1,SHAPE_CIRCLE, 0,14,0,FAMILY_LINUX,  20 },
 { L"Windows Vista",  C(24,30,40),  C(0,150,220),  C(0,150,220),   C(24,28,36),   C(0,150,220),  C(0,120,190),  40,0,SHAPE_ORB,    0, 0,1,FAMILY_WINDOWS, 21 },
 { L"Windows 95",     C(40,40,40),  C(0,128,128),  C(192,192,192), C(192,192,192),C(0,0,128),    C(192,192,192),28,0,SHAPE_TEXT98, 1, 0,0,FAMILY_WINDOWS, 22 },
};

const wchar_t *g_modeNames[7]   = { L"None", L"Dark", L"Light", L"AMOLED", L"Rose", L"Sapphire", L"Aero" };
const wchar_t *g_effectNames[6] = { L"None", L"Glass", L"iOS blur", L"3D", L"Boxed", L"Liquid glass" };

/* ---------- colour helpers ---------- */
COLORREF Col_Blend(COLORREF a, COLORREF b, int pct)
{
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    int r = (GetRValue(a) * (100 - pct) + GetRValue(b) * pct) / 100;
    int g = (GetGValue(a) * (100 - pct) + GetGValue(b) * pct) / 100;
    int bl= (GetBValue(a) * (100 - pct) + GetBValue(b) * pct) / 100;
    return RGB(r, g, bl);
}

int Col_Lum(COLORREF c)
{
    return (GetRValue(c) * 299 + GetGValue(c) * 587 + GetBValue(c) * 114) / 1000;
}

COLORREF Col_Text(COLORREF bg) { return Col_Lum(bg) > 150 ? RGB(18,18,18) : RGB(245,245,245); }

COLORREF Col_Tile(int i)
{
    static const COLORREF t[] = {
        RGB(0,120,215), RGB(16,137,62), RGB(194,57,179), RGB(232,17,35), RGB(255,140,0),
        RGB(0,153,188), RGB(122,117,116), RGB(135,100,184), RGB(0,183,195), RGB(202,80,16),
        RGB(77,89,143), RGB(0,178,148) };
    return t[((unsigned)i) % (sizeof t / sizeof t[0])];
}

int UiScale(int px)
{
    int s = g_cfg.p_uiscale; if (s < 50) s = 50; if (s > 250) s = 250;
    return (px * s + 50) / 100;
}

/* ---------- resolve ---------- */
void Theme_Resolve(Palette *p)
{
    int ti = g_cfg.p_theme; if (ti < 0 || ti >= THEME_COUNT) ti = 0;
    const ThemeDef *t = &g_themes[ti];
    int classic = g_cfg.p_classic;

    p->classic = classic;
    p->effect  = classic ? FX_NONE : g_cfg.p_effect;
    p->shape   = t->shape;
    p->centered= classic ? 0 : t->centered;
    p->glass   = classic ? t->glass : (g_cfg.p_effect == FX_GLASS || g_cfg.p_effect == FX_LIQUID || g_cfg.p_effect == FX_IOS_BLUR);
    p->bevel   = classic ? t->bevel : (g_cfg.p_effect == FX_3D);
    p->radius  = classic ? (t->shape == SHAPE_TEXTXP ? 6 : 0) : (g_cfg.p_round ? t->radius : 0);
    p->height  = classic ? t->cHeight : (t->cHeight < 36 ? 36 : t->cHeight);

    COLORREF bg  = classic ? t->cBg     : t->mBg;
    COLORREF acc = classic ? t->cAccent : t->mAccent;
    COLORREF st  = classic ? t->cStart  : t->mStart;

    if (!classic) {                                  /* colour modes only touch modern looks */
        switch (g_cfg.p_mode) {
        case 1: bg = Col_Blend(bg, RGB(0,0,0), 55);  break;                 /* dark */
        case 2: bg = Col_Blend(bg, RGB(245,245,245), 88); break;            /* light */
        case 3: bg = RGB(0,0,0); break;                                     /* amoled */
        case 4: bg = RGB(52,20,32); acc = RGB(232,90,130); st = acc; break; /* rose */
        case 5: bg = RGB(10,20,52); acc = RGB(15,82,186);  st = acc; break; /* sapphire */
        case 6: bg = RGB(60,110,160); acc = RGB(120,200,255); st = acc; p->glass = 1; break; /* aero */
        }
    }
    if (g_cfg.p_accent != -1) { acc = (COLORREF)g_cfg.p_accent; if (!classic) st = acc; }
    if (g_cfg.tb_color != -1)   bg  = (COLORREF)g_cfg.tb_color;
    if (g_cfg.st_btn_color != -1) st = (COLORREF)g_cfg.st_btn_color;

    p->bg      = bg;
    p->accent  = acc;
    p->startBg = st;
    p->fg      = Col_Text(bg);                       /* auto-adapt text to background */
    p->fgDim   = Col_Blend(p->fg, bg, 38);
    p->startFg = Col_Text(st);
    p->panel   = Col_Blend(bg, p->fg, 4);
    p->panelFg = Col_Text(p->panel);
    p->tile    = Col_Blend(bg, acc, 35);
    p->hover   = Col_Blend(bg, p->fg, 14);
    p->border  = Col_Blend(bg, p->fg, 22);
    p->shadow  = RGB(0,0,0);
    if (classic && t->bevel) {                       /* Win9x grey: dark text, grey panel */
        p->panel = bg; p->panelFg = RGB(0,0,0); p->fg = RGB(0,0,0);
        p->fgDim = RGB(96,96,96); p->hover = Col_Blend(bg, RGB(255,255,255), 40);
        p->border = RGB(128,128,128);
    }
}

/* ---------- fonts (cached per size/bold/name/scale) ---------- */
HFONT Theme_Font(int pt, BOOL bold)
{
    static struct { int key; HFONT f; wchar_t name[32]; } cache[24];
    int px = -MulDiv(pt, 96, 72);
    px = px * (g_cfg.p_uiscale < 50 ? 50 : g_cfg.p_uiscale) / 100;
    int key = px * 2 + (bold ? 1 : 0);
    for (int i = 0; i < 24; i++)
        if (cache[i].f && cache[i].key == key && !lstrcmpW(cache[i].name, g_cfg.font_name))
            return cache[i].f;
    static int next;
    int slot = next++ % 24;
    if (cache[slot].f) DeleteObject(cache[slot].f);
    cache[slot].f = CreateFontW(px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH, g_cfg.font_name);
    cache[slot].key = key;
    lstrcpynW(cache[slot].name, g_cfg.font_name, 32);
    return cache[slot].f;
}
