/* preview.c - live window preview on taskbar hover, and the tray-icon grid popup. */
#include "app.h"
#include <string.h>

#define PV_TIMER 1
#define TG_TIMER 1

static HWND    g_pv, g_tg;
static HWND    g_pvTarget;
static RECT    g_pvAnchor, g_tgAnchor;
static HBITMAP g_pvBmp; static int g_pvBw, g_pvBh;
static int     g_pvTick, g_pvHotClose;

static void PlaceNear(const RECT *a, int w, int h, int *x, int *y)
{
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN), gap = 6;
    switch (g_cfg.tb_position) {
    case 0:  *x = (a->left + a->right) / 2 - w / 2; *y = a->top - h - gap; break;      /* bar at bottom: above */
    case 1:  *x = (a->left + a->right) / 2 - w / 2; *y = a->bottom + gap;  break;
    case 2:  *x = a->right + gap; *y = (a->top + a->bottom) / 2 - h / 2;   break;
    default: *x = a->left - w - gap; *y = (a->top + a->bottom) / 2 - h / 2; break;
    }
    if (*x + w > sw) *x = sw - w; if (*x < 0) *x = 0;
    if (*y + h > sh) *y = sh - h; if (*y < 0) *y = 0;
}

/* ================================================================ preview */
static void Capture(void)
{
    if (!g_pvTarget || !IsWindow(g_pvTarget) || IsIconic(g_pvTarget)) {
        if (g_pvBmp) { DeleteObject(g_pvBmp); g_pvBmp = NULL; }
        return;
    }
    RECT r; GetWindowRect(g_pvTarget, &r);
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w < 2 || h < 2 || w > 8000 || h > 8000) return;
    HDC sdc = GetDC(NULL);
    HDC m = CreateCompatibleDC(sdc);
    HBITMAP bm = CreateCompatibleBitmap(sdc, w, h);
    HGDIOBJ old = SelectObject(m, bm);
    if (!PrintWindow(g_pvTarget, m, 0))                       /* fall back to a screen grab (needs the window visible) */
        BitBlt(m, 0, 0, w, h, sdc, r.left, r.top, SRCCOPY);
    SelectObject(m, old); DeleteDC(m); ReleaseDC(NULL, sdc);
    if (g_pvBmp) DeleteObject(g_pvBmp);
    g_pvBmp = bm; g_pvBw = w; g_pvBh = h;
}

static void PvCloseRect(RECT *c) { GetClientRect(g_pv, c); c->left = c->right - UiScale(26); c->bottom = UiScale(26); }

static void PvPaint(HDC wdc)
{
    Palette p; Theme_Resolve(&p);
    RECT rc; GetClientRect(g_pv, &rc);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bm = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(dc, bm);

    Draw_Surface(dc, &rc, &p, Col_Blend(p.bg, p.fg, 6), g_cfg.p_fx_preview);
    Draw_Round(dc, &rc, Col_Blend(p.bg, p.fg, 6), p.border, p.radius);
    int hdr = UiScale(26);
    wchar_t title[96] = L""; if (g_pvTarget) GetWindowTextW(g_pvTarget, title, 96);
    Draw_Icon(dc, UiScale(8), (hdr - 16) / 2, 16, g_pvTarget ? Icon_ForWindow(g_pvTarget) : NULL, &p);
    RECT t = { UiScale(30), 0, rc.right - hdr - 4, hdr };
    Draw_Text(dc, &t, title, p.fg, Theme_Font(g_cfg.p_fontsize, FALSE), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    RECT cl; PvCloseRect(&cl);
    if (g_pvHotClose) Draw_Round(dc, &cl, RGB(196,43,28), CLR_NONE, p.radius / 2);
    Draw_Glyph(dc, &cl, G_CLOSE, g_pvHotClose ? RGB(255,255,255) : p.fg, g_cfg.p_iconpreset);

    RECT body = { 8, hdr, rc.right - 8, rc.bottom - 8 };
    int bw = body.right - body.left, bh = body.bottom - body.top;
    if (g_pvBmp && g_pvBw > 0 && g_pvBh > 0) {
        int dw = bw, dh = bw * g_pvBh / g_pvBw;                /* keep the window's aspect ratio */
        if (dh > bh) { dh = bh; dw = bh * g_pvBw / g_pvBh; }
        int dx = body.left + (bw - dw) / 2, dy = body.top + (bh - dh) / 2;
        HDC m = CreateCompatibleDC(dc);
        HGDIOBJ o = SelectObject(m, g_pvBmp);
        SetStretchBltMode(dc, HALFTONE); SetBrushOrgEx(dc, 0, 0, NULL);
        StretchBlt(dc, dx, dy, dw, dh, m, 0, 0, g_pvBw, g_pvBh, SRCCOPY);
        SelectObject(m, o); DeleteDC(m);
    } else {                                                   /* minimised: no pixels to show */
        Draw_Text(dc, &body, L"Minimized", p.fgDim, Theme_Font(g_cfg.p_fontsize, FALSE), DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob); DeleteObject(bm); DeleteDC(dc);
}

static LRESULT CALLBACK PvProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); PvPaint(dc); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: {
        POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) }; RECT c; PvCloseRect(&c);
        int hot = PtInRect(&c, pt);
        if (hot != g_pvHotClose) { g_pvHotClose = hot; InvalidateRect(h, NULL, FALSE); }
        return 0; }
    case WM_LBUTTONUP: {
        POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) }; RECT c; PvCloseRect(&c);
        HWND t = g_pvTarget;
        Preview_Hide();
        if (!t || !IsWindow(t)) return 0;
        if (PtInRect(&c, pt)) PostMessageW(t, WM_CLOSE, 0, 0);
        else { if (IsIconic(t)) ShowWindow(t, SW_RESTORE); SetForegroundWindow(t); BringWindowToTop(t); }
        return 0; }
    case WM_TIMER: {
        POINT cp; GetCursorPos(&cp);
        RECT me, a = g_pvAnchor; GetWindowRect(h, &me);
        InflateRect(&a, 2, 2);
        if (!PtInRect(&me, cp) && !PtInRect(&a, cp)) { Preview_Hide(); return 0; }    /* mouse left both */
        if (!g_pvTarget || !IsWindow(g_pvTarget)) { Preview_Hide(); return 0; }
        if (++g_pvTick % 4 == 0) { Capture(); InvalidateRect(h, NULL, FALSE); }      /* "live": ~every 0.6 s */
        return 0; }
    }
    return DefWindowProcW(h, m, wp, lp);
}

void Preview_Show(HWND target, const RECT *anchor)
{
    if (!g_cfg.tb_preview) return;
    if (!g_pv) {
        WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
        wc.lpfnWndProc = PvProc; wc.hInstance = g_hInst; wc.lpszClassName = L"MontyPreview"; wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        g_pv = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, wc.lpszClassName, L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, g_hInst, NULL);
    }
    g_pvTarget = target; g_pvAnchor = *anchor; g_pvTick = 0; g_pvHotClose = 0;
    int w = UiScale(g_cfg.tb_prev_w), h = UiScale(g_cfg.tb_prev_h) + UiScale(26) + 8, x, y;
    PlaceNear(anchor, w, h, &x, &y);
    Capture();
    SetWindowPos(g_pv, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    Win_SetRound(g_pv, g_cfg.p_round ? 8 : 0);
    InvalidateRect(g_pv, NULL, FALSE);
    SetTimer(g_pv, PV_TIMER, 150, NULL);
}

void Preview_Hide(void)
{
    if (!g_pv) return;
    KillTimer(g_pv, PV_TIMER);
    ShowWindow(g_pv, SW_HIDE);
    if (g_pvBmp) { DeleteObject(g_pvBmp); g_pvBmp = NULL; }   /* free the bitmap: saves memory while idle */
    g_pvTarget = NULL;
}

BOOL Preview_IsVisible(void) { return g_pv && IsWindowVisible(g_pv); }

/* ================================================================ tray grid */
static int TgCols(int n) { return n < 4 ? (n ? n : 1) : 4; }

static void TgPaint(HDC wdc)
{
    Palette p; Theme_Resolve(&p);
    RECT rc; GetClientRect(g_tg, &rc);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bm = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(dc, bm);
    Draw_Surface(dc, &rc, &p, Col_Blend(p.bg, p.fg, 6), g_cfg.p_fx_taskbar);
    Draw_Round(dc, &rc, Col_Blend(p.bg, p.fg, 6), p.border, p.radius);
    int n = AppList_Count(LIST_TRAY), cols = TgCols(n), cell = UiScale(44);
    POINT cp; GetCursorPos(&cp); ScreenToClient(g_tg, &cp);
    for (int i = 0; i < n; i++) {
        RECT c = { 6 + (i % cols) * cell, 6 + (i / cols) * cell, 6 + (i % cols + 1) * cell, 6 + (i / cols + 1) * cell };
        if (PtInRect(&c, cp)) Draw_Round(dc, &c, p.hover, CLR_NONE, p.radius / 2);
        Draw_Icon(dc, (c.left + c.right) / 2 - 12, (c.top + c.bottom) / 2 - 12, 24, Icon_ForPath(AppList_Get(LIST_TRAY, i), TRUE), &p);
    }
    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob); DeleteObject(bm); DeleteDC(dc);
}

static int TgHit(LPARAM lp)
{
    int n = AppList_Count(LIST_TRAY), cols = TgCols(n), cell = UiScale(44);
    int x = (short)LOWORD(lp) - 6, y = (short)HIWORD(lp) - 6;
    if (x < 0 || y < 0) return -1;
    int i = (y / cell) * cols + (x / cell);
    return (x / cell < cols && i < n) ? i : -1;
}

static LRESULT CALLBACK TgProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); TgPaint(dc); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: InvalidateRect(h, NULL, FALSE); return 0;
    case WM_LBUTTONUP: { int i = TgHit(lp); if (i >= 0) { Shell_Run(AppList_Get(LIST_TRAY, i)); TrayGrid_Hide(); } return 0; }
    case WM_RBUTTONUP: {
        int i = TgHit(lp);
        if (i >= 0) {
            HMENU mm = CreatePopupMenu(); AppendMenuW(mm, MF_STRING, 1, L"Remove from tray");
            POINT pt; GetCursorPos(&pt); SetForegroundWindow(h);
            if (TrackPopupMenu(mm, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, NULL) == 1) {
                AppList_Remove(LIST_TRAY, i); Settings_Save(); TrayGrid_Hide(); PostMessageW(g_hTaskbar, WM_APP_REFRESH, 0, 0);
            }
            DestroyMenu(mm);
        }
        return 0; }
    case WM_TIMER: {                                         /* click outside closes the grid */
        POINT cp; GetCursorPos(&cp); RECT r, a = g_tgAnchor; GetWindowRect(h, &r);
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && !PtInRect(&r, cp) && !PtInRect(&a, cp)) TrayGrid_Hide();
        return 0; }
    }
    return DefWindowProcW(h, m, wp, lp);
}

void TrayGrid_Hide(void) { if (g_tg) { KillTimer(g_tg, TG_TIMER); ShowWindow(g_tg, SW_HIDE); } }

void TrayGrid_Toggle(const RECT *anchor)
{
    if (g_tg && IsWindowVisible(g_tg)) { TrayGrid_Hide(); return; }
    if (!g_tg) {
        WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
        wc.lpfnWndProc = TgProc; wc.hInstance = g_hInst; wc.lpszClassName = L"MontyTrayGrid"; wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        g_tg = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, wc.lpszClassName, L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, g_hInst, NULL);
    }
    g_tgAnchor = *anchor;
    int n = AppList_Count(LIST_TRAY), cols = TgCols(n), rows = (n + cols - 1) / (cols ? cols : 1), cell = UiScale(44);
    int w = cols * cell + 12, h = (rows ? rows : 1) * cell + 12, x, y;
    PlaceNear(anchor, w, h, &x, &y);
    SetWindowPos(g_tg, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    Win_SetRound(g_tg, g_cfg.p_round ? 8 : 0);
    InvalidateRect(g_tg, NULL, FALSE);
    SetTimer(g_tg, TG_TIMER, 100, NULL);
}
