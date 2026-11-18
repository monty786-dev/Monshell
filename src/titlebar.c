/* titlebar.c - custom title bars.
 * One borderless overlay per application window, sitting exactly over the window's native
 * title strip and kept directly ABOVE that window in z-order (so it is in front of the native bar
 * whether the window is focused or not, and behind any window that is above the app). */
#include "app.h"
#include <string.h>

#define MAXOV 48
#define TT_TIMER 1

typedef struct {
    HWND ov, target;
    RECT rc;              /* last placed rect, to avoid needless moves */
    unsigned sig;         /* title/focus/state signature */
    int hot;              /* button under the mouse, -1 none */
    int down;
    int btnN; struct { int id; RECT r; } btn[6];
} Ov;

enum { B_MIN, B_MAX, B_CLOSE, B_PIN, B_SHADE };
static Ov    g_ov[MAXOV];
static HWND  g_msgWnd;
static ATOM  g_cls;

/* ---------------- theme selection for title bars ---------------- */
static void TitlePalette(Palette *p, const ThemeDef **td)
{
    int saved = g_cfg.p_theme;
    int ti = g_cfg.tt_theme > 0 && g_cfg.tt_theme < THEME_COUNT ? g_cfg.tt_theme : saved;
    g_cfg.p_theme = ti;
    Theme_Resolve(p);
    g_cfg.p_theme = saved;
    if (td) *td = &g_themes[ti];
}

/* height of the native frame-top + caption (NOT the menu bar, which sits below the caption) */
static int NativeTopHeight(HWND w)
{
    RECT wr; GetWindowRect(w, &wr);
    TITLEBARINFO ti; ZeroMemory(&ti, sizeof ti); ti.cbSize = sizeof ti;
    if (GetTitleBarInfo(w, &ti) && !(ti.rgstate[0] & STATE_SYSTEM_INVISIBLE) && ti.rcTitleBar.bottom > ti.rcTitleBar.top) {
        int h = ti.rcTitleBar.bottom - wr.top;
        if (h > 0 && h < 200) return h;
    }
    LONG st = GetWindowLongW(w, GWL_STYLE);                     /* fallback: classic metrics */
    int frame = (st & WS_THICKFRAME) ? GetSystemMetrics(SM_CYSIZEFRAME) : GetSystemMetrics(SM_CYFIXEDFRAME);
    return GetSystemMetrics(SM_CYCAPTION) + frame;
}

/* ---------------- drawing ---------------- */
static void Layout(Ov *o, const RECT *rc, const ThemeDef *td, const Palette *pal, BOOL mac)
{
    int h = rc->bottom - rc->top, bs = h - 6; if (bs < 14) bs = 14;
    int bw = (pal->classic && pal->bevel) ? 16 : (mac ? bs - 4 : (td->shape == SHAPE_TEXTXP ? bs : bs + 14));
    LONG st = GetWindowLongW(o->target, GWL_STYLE);
    int ids[5], n = 0;
    if (mac) { ids[n++] = B_CLOSE; ids[n++] = B_MIN; ids[n++] = B_MAX; }
    else {
        if (g_cfg.tt_btn_pin)   ids[n++] = B_PIN;
        if (g_cfg.tt_btn_shade) ids[n++] = B_SHADE;
        if (st & WS_MINIMIZEBOX) ids[n++] = B_MIN;
        if (st & WS_MAXIMIZEBOX) ids[n++] = B_MAX;
        ids[n++] = B_CLOSE;
    }
    o->btnN = 0;
    if (mac) {
        int x = rc->left + 10;
        for (int i = 0; i < n; i++) { o->btn[o->btnN].id = ids[i]; o->btn[o->btnN].r = (RECT){ x, rc->top + (h - bw) / 2, x + bw, rc->top + (h - bw) / 2 + bw }; o->btnN++; x += bw + 8; }
    } else {
        int x = rc->right - 4;
        for (int i = n - 1; i >= 0; i--) {
            int w = bw; if (ids[i] == B_PIN || ids[i] == B_SHADE) w = bw - 8;
            RECT r = { x - w, rc->top + (h - bs) / 2, x, rc->top + (h - bs) / 2 + bs };
            if (!(pal->classic && pal->bevel) && td->shape != SHAPE_TEXTXP) { r.top = rc->top; r.bottom = rc->bottom - (td->shape == SHAPE_ORB ? 6 : 0); }
            o->btn[o->btnN].id = ids[i]; o->btn[o->btnN].r = r; o->btnN++;
            x -= w + ((pal->classic && pal->bevel) ? (ids[i] == B_MAX ? 2 : 0) : (td->shape == SHAPE_TEXTXP ? 3 : 0));
        }
    }
}

static void Paint(Ov *o, HDC wdc)
{
    Palette pal; const ThemeDef *td; TitlePalette(&pal, &td);
    RECT rc; GetClientRect(o->ov, &rc);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bm = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(dc, bm);

    BOOL active = GetForegroundWindow() == o->target;
    BOOL zoomed = IsZoomed(o->target);
    BOOL mac = td->family == FAMILY_APPLE && !pal.classic;
    BOOL w9x = pal.classic && pal.bevel;
    BOOL xp = td->shape == SHAPE_TEXTXP;
    BOOL aero = td->shape == SHAPE_ORB;
    Layout(o, &rc, td, &pal, mac);

    COLORREF bar = pal.bg, fg;
    if (g_cfg.tt_color != -1) bar = (COLORREF)g_cfg.tt_color;
    HFONT f = Theme_Font(g_cfg.p_fontsize, TRUE);

    if (w9x) {                                                   /* Win 95/98: navy -> light blue gradient */
        COLORREF a = g_cfg.tt_color != -1 ? bar : (active ? RGB(0,0,128) : RGB(128,128,128));
        COLORREF b = g_cfg.tt_color != -1 ? Col_Blend(bar, RGB(255,255,255), 40) : (active ? RGB(16,132,208) : RGB(181,181,181));
        RECT g = rc; Draw_Gradient(dc, &g, a, b, FALSE);
        fg = RGB(255,255,255);
    } else if (xp) {                                             /* Luna blue */
        COLORREF a = g_cfg.tt_color != -1 ? bar : (active ? RGB(0,88,238) : RGB(122,150,223));
        COLORREF b = Col_Blend(a, RGB(255,255,255), 30);
        Draw_Gradient(dc, &rc, b, a, TRUE);
        RECT hl = { 0, 0, rc.right, 1 }; Draw_Fill(dc, &hl, Col_Blend(a, RGB(255,255,255), 60));
        fg = RGB(255,255,255);
    } else if (aero) {                                           /* glass */
        COLORREF a = active ? Col_Blend(pal.bg, pal.accent, 45) : Col_Blend(pal.bg, RGB(120,120,120), 30);
        if (g_cfg.tt_color != -1) a = bar;
        Draw_Gradient(dc, &rc, Col_Blend(a, RGB(255,255,255), 30), a, TRUE);
        RECT tp = rc; tp.bottom = rc.top + (rc.bottom - rc.top) / 2; Draw_Alpha(dc, &tp, RGB(255,255,255), 35);
        fg = Col_Text(a);
    } else {                                                     /* flat families */
        COLORREF a = active ? bar : Col_Blend(bar, pal.fg, 6);
        if (td->family == FAMILY_APPLE && !pal.classic) a = active ? Col_Blend(bar, RGB(255,255,255), 6) : bar;
        Draw_Fill(dc, &rc, a);
        if (g_cfg.p_effect == FX_GLASS || g_cfg.p_effect == FX_LIQUID) {
            RECT tp = rc; tp.bottom = rc.top + (rc.bottom - rc.top) / 2;
            if (g_cfg.p_fx_title) Draw_Alpha(dc, &tp, RGB(255,255,255), g_cfg.p_shine * 40 / 100);
        } else if (g_cfg.p_effect == FX_3D && g_cfg.p_fx_title) Draw_Gradient(dc, &rc, Col_Blend(a, RGB(255,255,255), 15), Col_Blend(a, RGB(0,0,0), 15), TRUE);
        fg = Col_Text(a);
        if (!active) fg = Col_Blend(fg, a, 40);
        if (td->shape == SHAPE_LOGO || td->shape == SHAPE_CIRCLE || td->shape == SHAPE_SQUARE) {   /* accent line */
            RECT ln = { 0, rc.bottom - 1, rc.right, rc.bottom }; Draw_Fill(dc, &ln, active ? pal.accent : pal.border);
        }
    }
    if (g_cfg.tt_apply_all) {                                   /* border accent around the bar */
        HBRUSH fb = CreateSolidBrush(active ? pal.accent : pal.border); FrameRect(dc, &rc, fb); DeleteObject(fb);
    }

    /* icon + title */
    int ix = (mac && o->btnN) ? o->btn[o->btnN - 1].r.right + 12 : 8;
    int ic = rc.bottom - 10 < 16 ? 16 : (rc.bottom - 10 > 20 ? 20 : rc.bottom - 10);
    BOOL centered = mac || td->family == FAMILY_APPLE;
    wchar_t title[128]; GetWindowTextW(o->target, title, 128);
    RECT tr = { ix + (centered ? 0 : ic + 6), 0, rc.right, rc.bottom };
    int rightLimit = rc.right;
    for (int i = 0; i < o->btnN; i++) if (!mac && o->btn[i].r.left < rightLimit) rightLimit = o->btn[i].r.left;
    tr.right = rightLimit - 4;
    if (!centered) Draw_Icon(dc, ix, (rc.bottom - ic) / 2, ic, Icon_ForWindow(o->target), &pal);
    Draw_Text(dc, &tr, title, fg, f, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | (centered ? DT_CENTER : DT_LEFT));

    /* buttons */
    for (int i = 0; i < o->btnN; i++) {
        RECT b = o->btn[i].r; int id = o->btn[i].id; BOOL hot = (o->hot == i), dn = hot && o->down == i;
        int glyph = id == B_MIN ? G_MIN : id == B_MAX ? (zoomed ? G_RESTORE : G_MAX) : id == B_CLOSE ? G_CLOSE : id == B_PIN ? G_PIN : G_SHADE;
        COLORREF gc = fg;
        if (mac) {
            COLORREF c = id == B_CLOSE ? RGB(255,95,86) : id == B_MIN ? RGB(255,189,46) : RGB(39,201,63);
            if (!active && !hot) c = RGB(120,120,120);
            Draw_Round(dc, &b, c, CLR_NONE, (b.right - b.left) / 2);
            if (hot) Draw_Glyph(dc, &b, glyph, RGB(60,30,30), 0);
            continue;
        }
        if (w9x) {
            Draw_Fill(dc, &b, RGB(192,192,192)); Draw_Bevel(dc, &b, !dn);
            RECT g = b; if (dn) OffsetRect(&g, 1, 1);
            Draw_Glyph(dc, &g, glyph, RGB(0,0,0), 1);
        } else if (xp) {
            COLORREF c = id == B_CLOSE ? RGB(224,84,52) : RGB(40,110,240);
            Draw_Round(dc, &b, hot ? Col_Blend(c, RGB(255,255,255), 25) : c, RGB(255,255,255), 3);
            Draw_Glyph(dc, &b, glyph, RGB(255,255,255), 1);
        } else if (aero) {
            if (id == B_CLOSE) { Draw_Round(dc, &b, hot ? RGB(232,64,40) : RGB(199,80,80), CLR_NONE, 4); gc = RGB(255,255,255); }
            else if (hot) Draw_Round(dc, &b, Col_Blend(pal.accent, RGB(255,255,255), 40), CLR_NONE, 4);
            Draw_Glyph(dc, &b, glyph, gc, g_cfg.p_iconpreset);
        } else if (td->shape == SHAPE_CIRCLE) {                  /* Ubuntu / elementary: round buttons */
            RECT c = b; int s = c.bottom - c.top - 12; if (s < 10) s = 10;
            c = (RECT){ (b.left + b.right) / 2 - s / 2, (b.top + b.bottom) / 2 - s / 2, (b.left + b.right) / 2 + s / 2, (b.top + b.bottom) / 2 + s / 2 };
            Draw_Round(dc, &c, id == B_CLOSE ? pal.accent : (hot ? pal.hover : Col_Blend(bar, fg, 12)), CLR_NONE, s / 2);
            Draw_Glyph(dc, &c, glyph, id == B_CLOSE ? Col_Text(pal.accent) : fg, g_cfg.p_iconpreset);
        } else {                                                 /* Win 8/10/11, KDE, Arch, XFCE...: flat */
            if (id == B_CLOSE && hot) { Draw_Fill(dc, &b, RGB(232,17,35)); gc = RGB(255,255,255); }
            else if (hot) Draw_Fill(dc, &b, Col_Blend(bar, fg, 18));
            Draw_Glyph(dc, &b, glyph, gc, g_cfg.p_iconpreset);
        }
    }
    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob); DeleteObject(bm); DeleteDC(dc);
}

/* ---------------- actions ---------------- */
static void Shade(HWND t)
{
    RECT r; GetWindowRect(t, &r);
    HANDLE orig = GetPropW(t, L"MontyShadeH");
    if (orig) {                                                 /* unroll */
        SetWindowPos(t, NULL, 0, 0, r.right - r.left, (int)(INT_PTR)orig, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        RemovePropW(t, L"MontyShadeH");
    } else if (!IsZoomed(t)) {                                  /* roll up to the title strip */
        SetPropW(t, L"MontyShadeH", (HANDLE)(INT_PTR)(r.bottom - r.top));
        int nc = NativeTopHeight(t); if (nc < 20) nc = 24;
        SetWindowPos(t, NULL, 0, 0, r.right - r.left, nc, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

static void Pin(HWND t)
{
    BOOL top = (GetWindowLongW(t, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    SetWindowPos(t, top ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static void Button(Ov *o, int id)
{
    HWND t = o->target;
    switch (id) {
    case B_MIN:   ShowWindow(t, SW_MINIMIZE); break;
    case B_MAX:   ShowWindow(t, IsZoomed(t) ? SW_RESTORE : SW_MAXIMIZE); break;
    case B_CLOSE: PostMessageW(t, WM_CLOSE, 0, 0); break;
    case B_PIN:   Pin(t); break;
    case B_SHADE: Shade(t); break;
    }
}

static int BtnAt(Ov *o, int x, int y) { POINT p = { x, y }; for (int i = 0; i < o->btnN; i++) if (PtInRect(&o->btn[i].r, p)) return i; return -1; }

static Ov *FindOv(HWND h) { for (int i = 0; i < MAXOV; i++) if (g_ov[i].ov == h) return &g_ov[i]; return NULL; }

static LRESULT CALLBACK OvProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    Ov *o = FindOv(h);
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); if (o) Paint(o, dc); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: if (o) {
        int b = BtnAt(o, (short)LOWORD(lp), (short)HIWORD(lp));
        if (b != o->hot) { o->hot = b; InvalidateRect(h, NULL, FALSE); }
        TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 }; TrackMouseEvent(&t);
    } return 0;
    case WM_MOUSELEAVE: if (o) { o->hot = -1; InvalidateRect(h, NULL, FALSE); } return 0;
    case WM_LBUTTONDOWN: if (o) {
        int b = BtnAt(o, (short)LOWORD(lp), (short)HIWORD(lp));
        SetForegroundWindow(o->target);
        if (b >= 0) { o->down = b; SetCapture(h); InvalidateRect(h, NULL, FALSE); }
        else if (!IsZoomed(o->target)) {                         /* drag the window by its custom title */
            ReleaseCapture();
            POINT p; GetCursorPos(&p);
            PostMessageW(o->target, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(p.x, p.y));
        }
    } return 0;
    case WM_LBUTTONUP: if (o) {
        ReleaseCapture();
        int b = BtnAt(o, (short)LOWORD(lp), (short)HIWORD(lp)), d = o->down; o->down = -1;
        if (b >= 0 && b == d) Button(o, o->btn[b].id);
        InvalidateRect(h, NULL, FALSE);
    } return 0;
    case WM_LBUTTONDBLCLK: if (o && BtnAt(o, (short)LOWORD(lp), (short)HIWORD(lp)) < 0 && (GetWindowLongW(o->target, GWL_STYLE) & WS_MAXIMIZEBOX))
        ShowWindow(o->target, IsZoomed(o->target) ? SW_RESTORE : SW_MAXIMIZE); return 0;
    case WM_RBUTTONUP: if (o) {                                  /* system menu of the real window */
        HMENU sm = GetSystemMenu(o->target, FALSE); POINT p; GetCursorPos(&p);
        if (sm) { SetForegroundWindow(o->target); int c = TrackPopupMenu(sm, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, h, NULL); if (c) PostMessageW(o->target, WM_SYSCOMMAND, c, 0); }
    } return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* ---------------- tracking ---------------- */
static BOOL Eligible(HWND w, int *nc)
{
    if (!Shell_IsWindowAppWindow(w) || IsIconic(w)) return FALSE;
    LONG st = GetWindowLongW(w, GWL_STYLE);
    if ((st & WS_CAPTION) != WS_CAPTION) return FALSE;           /* only windows that really have a native title bar */
    if (GetPropW(w, L"MontyNoTitle")) return FALSE;
    *nc = NativeTopHeight(w);
    return *nc >= 12 && *nc < 120;
}

static Ov *GetOv(HWND target)
{
    Ov *free = NULL;
    for (int i = 0; i < MAXOV; i++) {
        if (g_ov[i].target == target) return &g_ov[i];
        if (!g_ov[i].target && !free) free = &g_ov[i];
    }
    if (!free) return NULL;
    if (!g_cls) {
        WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
        wc.lpfnWndProc = OvProc; wc.hInstance = g_hInst; wc.lpszClassName = L"MontyOverlay"; wc.style = CS_DBLCLKS; wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        g_cls = RegisterClassW(&wc);
    }
    free->ov = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"MontyOverlay", L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, g_hInst, NULL);
    if (!free->ov) return NULL;
    free->target = target; free->hot = -1; free->down = -1; free->sig = 0; SetRectEmpty(&free->rc);
    return free;
}

static void Release(Ov *o) { if (o->ov) DestroyWindow(o->ov); ZeroMemory(o, sizeof *o); }

void TitleBar_Tick(void)
{
    BOOL used[MAXOV] = { 0 };
    HWND fg = GetForegroundWindow();
    if (g_cfg.tt_enabled) {
        for (HWND w = GetTopWindow(NULL); w; w = GetWindow(w, GW_HWNDNEXT)) {
            int nc;
            if (!Eligible(w, &nc)) continue;
            Ov *o = GetOv(w); if (!o) continue;
            used[o - g_ov] = TRUE;

            RECT wr; GetWindowRect(w, &wr);
            RECT want = { wr.left, wr.top, wr.right, wr.top + nc };
            wchar_t t[64]; GetWindowTextW(w, t, 64);
            unsigned sig = 17u + (w == fg) * 3u + IsZoomed(w) * 7u + (unsigned)(GetWindowLongW(w, GWL_STYLE) & 0x30000);
            for (const wchar_t *c = t; *c; c++) sig = sig * 31u + *c;
            sig = sig * 13u + (unsigned)g_cfg.tt_theme * 101u + (unsigned)g_cfg.p_theme + (unsigned)g_cfg.p_effect * 7u + (unsigned)g_cfg.p_classic;

            BOOL moved = !EqualRect(&want, &o->rc);
            BOOL zFix = GetWindow(o->ov, GW_HWNDNEXT) != w;      /* keep directly above its window */
            if (moved || zFix) {
                HWND prev = GetWindow(w, GW_HWNDPREV);
                HWND after = prev ? prev : ((GetWindowLongW(w, GWL_EXSTYLE) & WS_EX_TOPMOST) ? HWND_TOPMOST : HWND_TOP);
                if (prev == o->ov) after = GetWindow(o->ov, GW_HWNDPREV) ? GetWindow(o->ov, GW_HWNDPREV) : HWND_TOP;
                SetWindowPos(o->ov, after, want.left, want.top, want.right - want.left, want.bottom - want.top,
                             SWP_NOACTIVATE | SWP_SHOWWINDOW | (moved ? 0 : SWP_NOMOVE | SWP_NOSIZE));
                o->rc = want; if (moved) InvalidateRect(o->ov, NULL, FALSE);
            }
            if (sig != o->sig) { o->sig = sig; Win_SetAlpha(o->ov, g_cfg.tt_alpha); InvalidateRect(o->ov, NULL, FALSE); }
        }
    }
    for (int i = 0; i < MAXOV; i++) if (g_ov[i].target && (!used[i] || !IsWindow(g_ov[i].target))) Release(&g_ov[i]);
}

void TitleBar_Refresh(void)
{
    for (int i = 0; i < MAXOV; i++) if (g_ov[i].target) { g_ov[i].sig = 0; Win_SetAlpha(g_ov[i].ov, g_cfg.tt_alpha); InvalidateRect(g_ov[i].ov, NULL, FALSE); }
    if (!g_cfg.tt_enabled) for (int i = 0; i < MAXOV; i++) if (g_ov[i].target) Release(&g_ov[i]);
    static int lastHide = -1;
    if (lastHide != g_cfg.tt_hide_native) { lastHide = g_cfg.tt_hide_native; Shell_ApplyWineTweaks(); }
    TitleBar_Tick();
}

static LRESULT CALLBACK MsgProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_TIMER && wp == TT_TIMER) {
        TitleBar_Tick();
        int ms = (g_cfg.perf_battery_lite && Power_OnBattery()) ? 160 : 80;
        static int cur = 80; if (ms != cur) { cur = ms; SetTimer(h, TT_TIMER, ms, NULL); }
        return 0;
    }
    if (m == WM_APP_REFRESH) { TitleBar_Refresh(); return 0; }
    if (m == WM_DESTROY) { for (int i = 0; i < MAXOV; i++) if (g_ov[i].target) Release(&g_ov[i]); return 0; }
    return DefWindowProcW(h, m, wp, lp);
}

BOOL TitleBar_Init(void)
{
    WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = MsgProc; wc.hInstance = g_hInst; wc.lpszClassName = L"MontyTitleHost";
    if (!RegisterClassW(&wc)) return FALSE;
    g_msgWnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, NULL);
    if (!g_msgWnd) return FALSE;
    SetTimer(g_msgWnd, TT_TIMER, 80, NULL);
    return TRUE;
}
