#include "draw.h"
#include "settings.h"
#include <shellapi.h>
#include <string.h>

const wchar_t *g_presetNames[ICON_PRESETS] = {
    L"Thin line", L"Bold line", L"Rounded", L"Outline circle", L"Filled circle", L"Square",
    L"Traffic dots", L"Classic 3D", L"Soft pill", L"Diamond", L"Duotone", L"Minimal",
    L"Neon", L"Pixel", L"Ring", L"Flat tile" };

/* per preset: stroke width, backing (0 none 1 circle 2 square 3 rounded 4 diamond), filled backing */
static const struct { int w, back, fill; } k_pre[ICON_PRESETS] = {
    {1,0,0},{3,0,0},{2,3,0},{1,1,0},{2,1,1},{2,2,0},{0,1,1},{2,2,1},
    {2,3,1},{2,4,0},{2,3,1},{1,0,0},{2,1,0},{3,2,1},{2,1,0},{2,2,1} };

/* ---------------- primitives ---------------- */
void Draw_Fill(HDC dc, const RECT *r, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, r, b);
    DeleteObject(b);
}

void Draw_Round(HDC dc, const RECT *r, COLORREF fill, COLORREF line, int rad)
{
    HBRUSH b = CreateSolidBrush(fill);
    HPEN   p = line == CLR_NONE ? (HPEN)GetStockObject(NULL_PEN) : CreatePen(PS_SOLID, 1, line);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    if (rad > 0) RoundRect(dc, r->left, r->top, r->right, r->bottom, rad * 2, rad * 2);
    else         Rectangle(dc, r->left, r->top, r->right, r->bottom);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b);
    if (line != CLR_NONE) DeleteObject(p);
}

void Draw_Bevel(HDC dc, const RECT *r, BOOL raised)
{
    COLORREF hi = RGB(255,255,255), lo = RGB(64,64,64), mid = RGB(128,128,128), face = RGB(223,223,223);
    COLORREF tl1 = raised ? face : lo, tl2 = raised ? hi : mid;
    COLORREF br1 = raised ? lo : hi,   br2 = raised ? mid : face;
    HPEN p;
    RECT q = *r; q.right--; q.bottom--;
    #define LINE(c,x1,y1,x2,y2) do{ p=CreatePen(PS_SOLID,1,c); HGDIOBJ o_=SelectObject(dc,p); MoveToEx(dc,x1,y1,NULL); LineTo(dc,x2,y2); SelectObject(dc,o_); DeleteObject(p); }while(0)
    HGDIOBJ old = SelectObject(dc, GetStockObject(BLACK_PEN));
    LINE(br1, q.left, q.bottom, q.right + 1, q.bottom); LINE(br1, q.right, q.top, q.right, q.bottom + 1);
    LINE(tl2, q.left, q.top, q.right, q.top);           LINE(tl2, q.left, q.top, q.left, q.bottom);
    LINE(br2, q.left + 1, q.bottom - 1, q.right, q.bottom - 1); LINE(br2, q.right - 1, q.top + 1, q.right - 1, q.bottom);
    LINE(tl1, q.left + 1, q.top + 1, q.right - 1, q.top + 1);    LINE(tl1, q.left + 1, q.top + 1, q.left + 1, q.bottom - 1);
    SelectObject(dc, old);
    #undef LINE
}

void Draw_Gradient(HDC dc, const RECT *r, COLORREF a, COLORREF b, BOOL vert)
{
    TRIVERTEX v[2] = {
        { r->left,  r->top,    (COLOR16)(GetRValue(a) << 8), (COLOR16)(GetGValue(a) << 8), (COLOR16)(GetBValue(a) << 8), 0 },
        { r->right, r->bottom, (COLOR16)(GetRValue(b) << 8), (COLOR16)(GetGValue(b) << 8), (COLOR16)(GetBValue(b) << 8), 0 } };
    GRADIENT_RECT g = { 0, 1 };
    GradientFill(dc, v, 2, &g, 1, vert ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H);
}

void Draw_Alpha(HDC dc, const RECT *r, COLORREF c, int alpha)
{
    int w = r->right - r->left, h = r->bottom - r->top;
    if (w <= 0 || h <= 0) return;
    if (alpha >= 255) { Draw_Fill(dc, r, c); return; }
    HDC m = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, 1, 1);
    HGDIOBJ old = SelectObject(m, bmp);
    SetPixel(m, 0, 0, c);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
    AlphaBlend(dc, r->left, r->top, w, h, m, 0, 0, 1, 1, bf);
    SelectObject(m, old); DeleteObject(bmp); DeleteDC(m);
}

void Draw_Text(HDC dc, const RECT *r, const wchar_t *s, COLORREF c, HFONT f, UINT fmt)
{
    HGDIOBJ of = SelectObject(dc, f ? f : Theme_Font(g_cfg.p_fontsize, FALSE));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    RECT q = *r;
    DrawTextW(dc, s, -1, &q, fmt | DT_NOPREFIX);
    SelectObject(dc, of);
}

void Draw_Shadow(HDC dc, const RECT *r, int spread)
{
    for (int i = spread; i > 0; i--) {
        RECT q = { r->left - i, r->top - i, r->right + i, r->bottom + i };
        Draw_Alpha(dc, &q, RGB(0,0,0), 6);
    }
}

/* ---------------- surface with effect ---------------- */
void Draw_Surface(HDC dc, const RECT *r, const Palette *p, COLORREF base, BOOL fx)
{
    int rad = p->radius;
    Draw_Round(dc, r, base, CLR_NONE, rad);
    if (!fx) return;

    int shine = g_cfg.p_shine, gloss = g_cfg.p_gloss;           /* 0..100 */
    RECT top = *r; top.bottom = r->top + (r->bottom - r->top) / 2;

    switch (p->effect) {
    case FX_GLASS:
    case FX_LIQUID:
        Draw_Alpha(dc, &top, RGB(255,255,255), shine * 70 / 100);            /* glass shine on upper half */
        { RECT low = *r; low.top = top.bottom; Draw_Alpha(dc, &low, RGB(0,0,0), gloss * 35 / 100); }
        if (p->effect == FX_LIQUID) {                                        /* soft rim light */
            RECT in = *r; InflateRect(&in, -1, -1);
            Draw_Round(dc, &in, base, Col_Blend(base, RGB(255,255,255), 35), rad);
            Draw_Alpha(dc, &top, RGB(255,255,255), shine * 40 / 100);
        }
        break;
    case FX_IOS_BLUR:                                                         /* frosted: lighten + fine rim */
        Draw_Alpha(dc, r, RGB(255,255,255), 10 + g_cfg.p_blur);
        Draw_Round(dc, r, Col_Blend(base, RGB(255,255,255), 8), Col_Blend(base, RGB(255,255,255), 30), rad);
        Draw_Alpha(dc, &top, RGB(255,255,255), shine * 25 / 100);
        break;
    case FX_3D:
        { RECT g1 = *r; Draw_Gradient(dc, &g1, Col_Blend(base, RGB(255,255,255), 18), Col_Blend(base, RGB(0,0,0), 25), TRUE); }
        Draw_Bevel(dc, r, TRUE);
        break;
    case FX_BOXED:
        Draw_Round(dc, r, base, p->accent, rad);
        { RECT in = *r; InflateRect(&in, -3, -3); Draw_Round(dc, &in, base, p->border, rad > 2 ? rad - 2 : 0); }
        break;
    }
}

/* ---------------- start button ---------------- */
void Draw_StartButton(HDC dc, const RECT *r, const Palette *p, BOOL hot, BOOL down)
{
    int w = r->right - r->left, h = r->bottom - r->top;
    int cx = (r->left + r->right) / 2, cy = (r->top + r->bottom) / 2;
    COLORREF st = hot ? Col_Blend(p->startBg, RGB(255,255,255), 20) : p->startBg;

    if (g_cfg.start_icon_path[0]) {                         /* user-chosen start button picture overrides the OS shape */
        HBITMAP pic = Img_Load(g_cfg.start_icon_path);
        if (pic) {
            if (hot || down) Draw_Round(dc, r, p->hover, CLR_NONE, p->radius / 2);
            RECT q = *r; InflateRect(&q, -4, -4);
            Img_Draw(dc, &q, pic, 1);
            return;
        }
    }
    switch (p->shape) {
    case SHAPE_TEXT98: {                                   /* Win 9x raised "Start" button */
        RECT q = *r; InflateRect(&q, -2, -2);
        Draw_Fill(dc, &q, p->bg);
        Draw_Bevel(dc, &q, !down);
        RECT t = q; if (down) OffsetRect(&t, 1, 1);
        t.left += 22;
        Draw_Text(dc, &t, L"Start", RGB(0,0,0), Theme_Font(9, TRUE), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        RECT f = { q.left + 5, cy - 6, q.left + 17, cy + 6 };
        if (down) OffsetRect(&f, 1, 1);
        RECT a = { f.left, f.top, f.left + 5, f.top + 5 };  Draw_Fill(dc, &a, RGB(255,0,0));
        RECT b = { f.left + 6, f.top, f.left + 12, f.top + 5 }; Draw_Fill(dc, &b, RGB(0,160,0));
        RECT c = { f.left, f.top + 6, f.left + 5, f.top + 12 }; Draw_Fill(dc, &c, RGB(0,0,255));
        RECT d = { f.left + 6, f.top + 6, f.left + 12, f.top + 12 }; Draw_Fill(dc, &d, RGB(255,200,0));
        break; }
    case SHAPE_TEXTXP: {                                   /* Luna green pill */
        RECT q = *r; q.right += 8;
        Draw_Gradient(dc, &q, Col_Blend(st, RGB(255,255,255), hot ? 35 : 22), Col_Blend(st, RGB(0,0,0), 22), TRUE);
        HRGN rg = CreateRoundRectRgn(q.right - 24, q.top, q.right + 24, q.bottom, 48, 48);
        HBRUSH bg = CreateSolidBrush(p->bg); FillRgn(dc, rg, bg); DeleteObject(bg); DeleteObject(rg);
        RECT t = *r; t.left += 28;
        Draw_Text(dc, &t, L"start", RGB(255,255,255), Theme_Font(11, TRUE), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        RECT f = { r->left + 6, cy - 8, r->left + 22, cy + 8 };
        Draw_Round(dc, &f, RGB(240,240,240), CLR_NONE, 3);
        break; }
    case SHAPE_ORB: {                                      /* Vista/7 orb */
        int rr = (h < w ? h : w) / 2 - 1;
        HBRUSH b = CreateSolidBrush(Col_Blend(st, RGB(0,0,0), 40));
        HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, cx - rr, cy - rr, cx + rr, cy + rr);
        SelectObject(dc, ob); DeleteObject(b);
        b = CreateSolidBrush(st); SelectObject(dc, b);
        Ellipse(dc, cx - rr + 2, cy - rr + 2, cx + rr - 2, cy + rr - 2);
        SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b);
        RECT hi = { cx - rr + 5, cy - rr + 4, cx + rr - 5, cy };
        Draw_Alpha(dc, &hi, RGB(255,255,255), 70);
        goto logo;
        }
    case SHAPE_CIRCLE: {
        int rr = (h < w ? h : w) / 2 - 3;
        COLORREF c2 = st;
        HBRUSH b = CreateSolidBrush(c2);
        HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, cx - rr, cy - rr, cx + rr, cy + rr);
        SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b);
        RECT in = { cx - rr / 2, cy - rr / 2, cx + rr / 2, cy + rr / 2 };
        Draw_Round(dc, &in, p->startFg, CLR_NONE, rr / 2);
        break; }
    case SHAPE_SQUARE: {
        RECT q = *r; InflateRect(&q, -3, -3);
        Draw_Round(dc, &q, st, CLR_NONE, p->radius);
        RECT in = { cx - 6, cy - 6, cx + 6, cy + 6 };
        Draw_Round(dc, &in, p->startFg, CLR_NONE, 2);
        break; }
    default:
        if (hot) { RECT q = *r; Draw_Fill(dc, &q, p->hover); }
    logo: {                                                 /* four-pane logo (Win 8/10/11, orb centre) */
        COLORREF lc = (p->shape == SHAPE_ORB) ? RGB(255,255,255) : p->accent;
        if (p->shape == SHAPE_LOGO && Col_Lum(p->bg) > 150) lc = p->accent;
        int s = (h < w ? h : w) / 4; if (s < 5) s = 5;
        RECT q1 = { cx - s, cy - s, cx - 1, cy - 1 }, q2 = { cx + 1, cy - s, cx + s, cy - 1 };
        RECT q3 = { cx - s, cy + 1, cx - 1, cy + s }, q4 = { cx + 1, cy + 1, cx + s, cy + s };
        Draw_Fill(dc, &q1, lc); Draw_Fill(dc, &q2, lc); Draw_Fill(dc, &q3, lc); Draw_Fill(dc, &q4, lc);
        } }
}

/* ---------------- glyphs (system icon presets) ---------------- */
void Draw_Glyph(HDC dc, const RECT *r, int g, COLORREF col, int preset)
{
    if (preset < 0 || preset >= ICON_PRESETS) preset = 0;
    int w = r->right - r->left, h = r->bottom - r->top, s = (w < h ? w : h);
    int cx = (r->left + r->right) / 2, cy = (r->top + r->bottom) / 2;
    int u = s / 2 - 4; if (u < 3) u = 3;                       /* half extent of the symbol */
    COLORREF back = (preset == 10 || preset == 15) ? Col_Blend(col, RGB(128,128,128), 70) : col;
    int pw = k_pre[preset].w ? k_pre[preset].w : 1;

    /* optional backing shape */
    if (k_pre[preset].back) {
        RECT b = { cx - u - 3, cy - u - 3, cx + u + 3, cy + u + 3 };
        COLORREF fill = k_pre[preset].fill ? Col_Blend(col, RGB(0,0,0), 82) : CLR_NONE;
        HBRUSH br = fill == CLR_NONE ? (HBRUSH)GetStockObject(NULL_BRUSH) : CreateSolidBrush(fill);
        HPEN pn = CreatePen(PS_SOLID, 1, back);
        HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pn);
        switch (k_pre[preset].back) {
        case 1: Ellipse(dc, b.left, b.top, b.right, b.bottom); break;
        case 2: Rectangle(dc, b.left, b.top, b.right, b.bottom); break;
        case 3: RoundRect(dc, b.left, b.top, b.right, b.bottom, 8, 8); break;
        case 4: { POINT d[4] = { {cx, b.top}, {b.right, cy}, {cx, b.bottom}, {b.left, cy} }; Polygon(dc, d, 4); break; }
        }
        SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pn);
        if (fill != CLR_NONE) DeleteObject(br);
    }

    HPEN pn = CreatePen(PS_SOLID, pw, col);
    HGDIOBJ op = SelectObject(dc, pn), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    #define L2(x1,y1,x2,y2) do{ MoveToEx(dc,(x1),(y1),NULL); LineTo(dc,(x2),(y2)); }while(0)
    switch (g) {
    case G_MIN:     L2(cx - u, cy + u / 2, cx + u + 1, cy + u / 2); break;
    case G_MAX:     Rectangle(dc, cx - u, cy - u, cx + u, cy + u); break;
    case G_RESTORE: Rectangle(dc, cx - u, cy - u / 2, cx + u / 2 + 1, cy + u);
                    L2(cx - u / 2, cy - u / 2, cx - u / 2, cy - u); L2(cx - u / 2, cy - u, cx + u, cy - u);
                    L2(cx + u, cy - u, cx + u, cy + u / 2); L2(cx + u, cy + u / 2, cx + u / 2, cy + u / 2); break;
    case G_CLOSE:   L2(cx - u, cy - u, cx + u + 1, cy + u + 1); L2(cx + u, cy - u, cx - u - 1, cy + u + 1); break;
    case G_PIN:     L2(cx, cy - u, cx, cy + u); L2(cx - u, cy - u / 2, cx + u, cy - u / 2); L2(cx - u / 2, cy - u, cx + u / 2, cy - u); break;
    case G_SHADE:   L2(cx - u, cy - u, cx + u + 1, cy - u); L2(cx - u, cy - u, cx, cy + u / 2); L2(cx + u, cy - u, cx, cy + u / 2); break;
    case G_POWER:   Arc(dc, cx - u, cy - u, cx + u, cy + u, cx + u / 2, cy - u, cx - u / 2, cy - u); L2(cx, cy - u - 1, cx, cy); break;
    case G_GEAR:    Ellipse(dc, cx - u, cy - u, cx + u, cy + u); Ellipse(dc, cx - u / 3, cy - u / 3, cx + u / 3, cy + u / 3);
                    L2(cx, cy - u - 2, cx, cy - u); L2(cx, cy + u, cx, cy + u + 2); L2(cx - u - 2, cy, cx - u, cy); L2(cx + u, cy, cx + u + 2, cy); break;
    case G_RUN:     L2(cx - u, cy - u, cx - u, cy + u); L2(cx - u, cy - u, cx + u, cy); L2(cx - u, cy + u, cx + u, cy); break;
    case G_BATTERY: Rectangle(dc, cx - u - 1, cy - u / 2, cx + u, cy + u / 2 + 1); L2(cx + u + 1, cy - 2, cx + u + 1, cy + 3); break;
    case G_WIFI:    Arc(dc, cx - u, cy - u + 2, cx + u, cy + u + 2, cx + u, cy, cx - u, cy);
                    Arc(dc, cx - u / 2, cy - u / 2 + 3, cx + u / 2, cy + u / 2 + 3, cx + u / 2, cy + 1, cx - u / 2, cy + 1);
                    L2(cx, cy + u - 1, cx, cy + u); break;
    case G_BT:      L2(cx, cy - u, cx, cy + u); L2(cx, cy - u, cx + u / 2 + 1, cy - u / 2); L2(cx + u / 2 + 1, cy - u / 2, cx - u / 2, cy + u / 2);
                    L2(cx - u / 2, cy - u / 2, cx + u / 2 + 1, cy + u / 2); L2(cx + u / 2 + 1, cy + u / 2, cx, cy + u); break;
    case G_TASKMGR: Rectangle(dc, cx - u, cy - u, cx + u, cy + u); L2(cx - u, cy + u / 3, cx - u / 3, cy + u / 3);
                    L2(cx - u / 3, cy + u / 3, cx, cy - u / 2); L2(cx, cy - u / 2, cx + u / 3, cy + u / 2); L2(cx + u / 3, cy + u / 2, cx + u, cy); break;
    case G_PLUS:    L2(cx - u, cy, cx + u + 1, cy); L2(cx, cy - u, cx, cy + u + 1); break;
    case G_CHEV_UP: L2(cx - u, cy + u / 2, cx, cy - u / 2); L2(cx, cy - u / 2, cx + u + 1, cy + u / 2); break;
    case G_CHEV_DOWN: L2(cx - u, cy - u / 2, cx, cy + u / 2); L2(cx, cy + u / 2, cx + u + 1, cy - u / 2); break;
    case G_DESKTOP: Rectangle(dc, cx - u, cy - u, cx + u, cy + u - 3); L2(cx - u / 2, cy + u, cx + u / 2, cy + u); break;
    case G_GRID:    Rectangle(dc, cx - u, cy - u, cx - 1, cy - 1); Rectangle(dc, cx + 1, cy - u, cx + u, cy - 1);
                    Rectangle(dc, cx - u, cy + 1, cx - 1, cy + u); Rectangle(dc, cx + 1, cy + 1, cx + u, cy + u); break;
    }
    #undef L2
    SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(pn);
}

/* ---------------- icons ---------------- */
#define ICON_CACHE 256
static struct { wchar_t path[MAX_PATH]; HICON small, large; } g_ic[ICON_CACHE];
static int g_icN;

HICON Icon_ForPath(const wchar_t *path, BOOL large)
{
    for (int i = 0; i < g_icN; i++)
        if (!lstrcmpiW(g_ic[i].path, path)) return large ? g_ic[i].large : g_ic[i].small;
    HICON s = NULL, l = NULL;
    ExtractIconExW(path, 0, &l, &s, 1);
    if (!s || !l) {                                       /* .lnk or missing icon: ask the shell */
        SHFILEINFOW fi; ZeroMemory(&fi, sizeof fi);
        if (SHGetFileInfoW(path, 0, &fi, sizeof fi, SHGFI_ICON | SHGFI_SMALLICON)) s = fi.hIcon;
        ZeroMemory(&fi, sizeof fi);
        if (SHGetFileInfoW(path, 0, &fi, sizeof fi, SHGFI_ICON | SHGFI_LARGEICON)) l = fi.hIcon;
    }
    if (!s) s = l; if (!l) l = s;
    if (!s) s = l = LoadIconW(NULL, IDI_APPLICATION);
    if (g_icN < ICON_CACHE) {
        lstrcpynW(g_ic[g_icN].path, path, MAX_PATH);
        g_ic[g_icN].small = s; g_ic[g_icN].large = l; g_icN++;
    }
    return large ? l : s;
}

HICON Icon_ForWindow(HWND w)
{
    DWORD_PTR r = 0;
    HICON ic = NULL;
    if (SendMessageTimeoutW(w, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 50, &r) && r) ic = (HICON)r;
    if (!ic && SendMessageTimeoutW(w, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 50, &r) && r) ic = (HICON)r;
    if (!ic) ic = (HICON)GetClassLongPtrW(w, GCLP_HICONSM);
    if (!ic) ic = (HICON)GetClassLongPtrW(w, GCLP_HICON);
    return ic ? ic : LoadIconW(NULL, IDI_APPLICATION);
}

static HBITMAP IconToDib(HICON ic, int size, void **bits)
{
    BITMAPINFO bi; ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = size; bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    HDC sdc = GetDC(NULL);
    HBITMAP bm = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, bits, NULL, 0);
    HDC m = CreateCompatibleDC(sdc);
    HGDIOBJ old = SelectObject(m, bm);
    if (*bits) memset(*bits, 0, (size_t)size * size * 4);
    DrawIconEx(m, 0, 0, ic, size, size, 0, NULL, DI_NORMAL);
    SelectObject(m, old); DeleteDC(m); ReleaseDC(NULL, sdc);
    return bm;
}

COLORREF Icon_Dominant(HICON ic, COLORREF fb)
{
    void *bits = NULL; const int S = 16;
    HBITMAP bm = IconToDib(ic, S, &bits);
    if (!bits) { if (bm) DeleteObject(bm); return fb; }
    unsigned long r = 0, g = 0, b = 0, n = 0;
    BYTE *px = (BYTE *)bits;
    for (int i = 0; i < S * S; i++) {
        BYTE B = px[i*4], G = px[i*4+1], R = px[i*4+2];
        int mx = R > G ? (R > B ? R : B) : (G > B ? G : B), mn = R < G ? (R < B ? R : B) : (G < B ? G : B);
        if (mx - mn < 40 || mx < 60) continue;               /* skip greys: want the "colourful" part */
        r += R; g += G; b += B; n++;
    }
    DeleteObject(bm);
    if (n < 4) return fb;
    return RGB(r / n, g / n, b / n);
}

void Draw_Icon(HDC dc, int x, int y, int size, HICON ic, const Palette *p)
{
    if (!ic) return;
    if (!g_cfg.p_icon_recolor) { DrawIconEx(dc, x, y, ic, size, size, 0, NULL, DI_NORMAL); return; }
    void *bits = NULL;
    HBITMAP bm = IconToDib(ic, size, &bits);
    if (!bits) { if (bm) DeleteObject(bm); DrawIconEx(dc, x, y, ic, size, size, 0, NULL, DI_NORMAL); return; }
    COLORREF tint = g_cfg.p_icon_fill ? p->accent : p->fg;     /* fill: accent colour, else text colour */
    BYTE *px = (BYTE *)bits;
    for (int i = 0; i < size * size; i++) {                    /* keep alpha, premultiply tint */
        BYTE a = px[i*4+3];
        if (!a) { px[i*4+3] = (px[i*4] | px[i*4+1] | px[i*4+2]) ? 255 : 0; a = px[i*4+3]; }
        px[i*4]   = GetBValue(tint) * a / 255;
        px[i*4+1] = GetGValue(tint) * a / 255;
        px[i*4+2] = GetRValue(tint) * a / 255;
    }
    HDC m = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(m, bm);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(dc, x, y, size, size, m, 0, 0, size, size, bf);
    SelectObject(m, old); DeleteDC(m); DeleteObject(bm);
}

void Win_SetAlpha(HWND w, int a)
{
    if (a >= 255) { SetWindowLongW(w, GWL_EXSTYLE, GetWindowLongW(w, GWL_EXSTYLE) & ~WS_EX_LAYERED); return; }
    SetWindowLongW(w, GWL_EXSTYLE, GetWindowLongW(w, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(w, 0, (BYTE)(a < 30 ? 30 : a), LWA_ALPHA);
}

void Win_SetRound(HWND w, int rad)
{
    if (rad <= 0) { SetWindowRgn(w, NULL, TRUE); return; }
    RECT r; GetWindowRect(w, &r);
    HRGN rg = CreateRoundRectRgn(0, 0, r.right - r.left + 1, r.bottom - r.top + 1, rad * 2, rad * 2);
    SetWindowRgn(w, rg, TRUE);
}

/* ---------------- pictures (PNG / JPG / BMP / GIF through GDI+, loaded at run time) ---------------- */
typedef struct { UINT32 ver; void *cb; BOOL noThread; BOOL noCodecs; } GpInput;
typedef int (WINAPI *PFN_Startup)(ULONG_PTR *, const GpInput *, void *);
typedef int (WINAPI *PFN_FromFile)(const WCHAR *, void **);
typedef int (WINAPI *PFN_ToHBitmap)(void *, HBITMAP *, DWORD);
typedef int (WINAPI *PFN_Dispose)(void *);

#define IMG_CACHE 24
static struct { wchar_t path[MAX_PATH]; HBITMAP bmp; } g_img[IMG_CACHE];
static int g_imgN;

HBITMAP Img_Load(const wchar_t *path)
{
    if (!path || !path[0]) return NULL;
    for (int i = 0; i < g_imgN; i++) if (!lstrcmpiW(g_img[i].path, path)) return g_img[i].bmp;
    HBITMAP hb = NULL;
    static HMODULE gp; static PFN_FromFile ff; static PFN_ToHBitmap th; static PFN_Dispose dp; static BOOL tried;
    if (!tried) {
        tried = TRUE;
        gp = LoadLibraryW(L"gdiplus.dll");
        if (gp) {
            PFN_Startup st = (PFN_Startup)GetProcAddress(gp, "GdiplusStartup");
            ff = (PFN_FromFile)GetProcAddress(gp, "GdipCreateBitmapFromFile");
            th = (PFN_ToHBitmap)GetProcAddress(gp, "GdipCreateHBITMAPFromBitmap");
            dp = (PFN_Dispose)GetProcAddress(gp, "GdipDisposeImage");
            ULONG_PTR tok; GpInput in = { 1, NULL, FALSE, FALSE };
            if (!st || st(&tok, &in, NULL) != 0) ff = NULL;
        }
    }
    if (ff && th) {
        void *bmp = NULL;
        if (ff(path, &bmp) == 0 && bmp) { th(bmp, &hb, 0); if (dp) dp(bmp); }
    }
    if (!hb) hb = (HBITMAP)LoadImageW(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);   /* plain BMP fallback */
    if (g_imgN < IMG_CACHE) { lstrcpynW(g_img[g_imgN].path, path, MAX_PATH); g_img[g_imgN].bmp = hb; g_imgN++; }
    return hb;
}

void Img_Forget(void)                                  /* call after the user picks a new picture */
{
    for (int i = 0; i < g_imgN; i++) if (g_img[i].bmp) DeleteObject(g_img[i].bmp);
    g_imgN = 0;
}

/* fit: 0 crop-to-fill, 1 fit inside, 2 stretch, 3 centre, 4 tile */
void Img_Draw(HDC dc, const RECT *r, HBITMAP bmp, int fit)
{
    if (!bmp) return;
    BITMAP b; GetObject(bmp, sizeof b, &b);
    int rw = r->right - r->left, rh = r->bottom - r->top;
    if (b.bmWidth <= 0 || b.bmHeight <= 0 || rw <= 0 || rh <= 0) return;
    HDC m = CreateCompatibleDC(dc);
    HGDIOBJ o = SelectObject(m, bmp);
    int save = SaveDC(dc);
    IntersectClipRect(dc, r->left, r->top, r->right, r->bottom);
    SetStretchBltMode(dc, HALFTONE); SetBrushOrgEx(dc, 0, 0, NULL);
    switch (fit) {
    case 2: StretchBlt(dc, r->left, r->top, rw, rh, m, 0, 0, b.bmWidth, b.bmHeight, SRCCOPY); break;
    case 3: BitBlt(dc, r->left + (rw - b.bmWidth) / 2, r->top + (rh - b.bmHeight) / 2, b.bmWidth, b.bmHeight, m, 0, 0, SRCCOPY); break;
    case 4: for (int y = r->top; y < r->bottom; y += b.bmHeight) for (int x = r->left; x < r->right; x += b.bmWidth)
                BitBlt(dc, x, y, b.bmWidth, b.bmHeight, m, 0, 0, SRCCOPY); break;
    default: {
        double sx = (double)rw / b.bmWidth, sy = (double)rh / b.bmHeight;
        double s = fit == 1 ? (sx < sy ? sx : sy) : (sx > sy ? sx : sy);
        int dw = (int)(b.bmWidth * s), dh = (int)(b.bmHeight * s);
        if (fit == 1) { RECT bg = *r; HBRUSH bb = CreateSolidBrush(RGB(0,0,0)); FillRect(dc, &bg, bb); DeleteObject(bb); }
        StretchBlt(dc, r->left + (rw - dw) / 2, r->top + (rh - dh) / 2, dw, dh, m, 0, 0, b.bmWidth, b.bmHeight, SRCCOPY);
        break; }
    }
    RestoreDC(dc, save);
    SelectObject(m, o); DeleteDC(m);
}

/* circular avatar */
void Img_DrawCircle(HDC dc, const RECT *r, HBITMAP bmp, COLORREF fallback)
{
    int save = SaveDC(dc);
    HRGN rg = CreateEllipticRgn(r->left, r->top, r->right, r->bottom);
    SelectClipRgn(dc, rg);
    if (bmp) Img_Draw(dc, r, bmp, 0); else Draw_Fill(dc, r, fallback);
    RestoreDC(dc, save);
    DeleteObject(rg);
}
