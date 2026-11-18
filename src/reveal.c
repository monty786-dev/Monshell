/* reveal.c - thin "reveal bar" at the screen edge. Hover it and the taskbar slides over the running
 * app for N seconds, then collapses again. Also owns the fullscreen / maximized detection that
 * decides how the taskbar behaves over such apps (keep / over app / floating icon / hide). */
#include "app.h"
#include <wchar.h>
#include <string.h>

#define RV_TIMER 1

HWND g_hReveal;
static BOOL  g_fsNow, g_peeking, g_barShown;
static DWORD g_peekEnd;
static HWND  g_fsWnd;

/* ---------------- exceptions (semicolon separated exe names in settings) ---------------- */
static void BaseName(const wchar_t *path, wchar_t *out, int n)
{
    const wchar_t *s = wcsrchr(path, L'\\'); s = s ? s + 1 : path;
    lstrcpynW(out, s, n); CharLowerW(out);
}

BOOL Reveal_IsException(const wchar_t *exe)
{
    if (!g_cfg.rv_exceptions[0] || !exe || !exe[0]) return FALSE;
    wchar_t b[MAX_PATH], list[512];
    BaseName(exe, b, MAX_PATH);
    lstrcpynW(list, g_cfg.rv_exceptions, 512); CharLowerW(list);
    const wchar_t *s = list;
    for (;;) {                                           /* semicolon separated, compare whole names */
        const wchar_t *e = wcschr(s, L';');
        int len = e ? (int)(e - s) : lstrlenW(s);
        if (len > 0 && (int)lstrlenW(b) == len && !wcsncmp(s, b, len)) return TRUE;
        if (!e) break;
        s = e + 1;
    }
    return FALSE;
}

void Reveal_AddException(const wchar_t *exe)
{
    wchar_t b[MAX_PATH]; BaseName(exe, b, MAX_PATH);
    if (!b[0] || Reveal_IsException(exe)) return;
    if (lstrlenW(g_cfg.rv_exceptions) + lstrlenW(b) + 2 >= 512) return;
    if (g_cfg.rv_exceptions[0]) lstrcatW(g_cfg.rv_exceptions, L";");
    lstrcatW(g_cfg.rv_exceptions, b);
    Settings_Save();
}

/* ---------------- fullscreen / maximized detection ---------------- */
BOOL Reveal_IsFullscreenApp(HWND *app)
{
    HWND fg = GetForegroundWindow();
    if (app) *app = NULL;
    if (!fg || fg == g_hTaskbar || fg == g_hStart || fg == g_hReveal || fg == GetDesktopWindow() || fg == GetShellWindow()) return FALSE;
    wchar_t cls[48]; GetClassNameW(fg, cls, 48);
    if (!lstrcmpW(cls, L"Progman") || !lstrcmpW(cls, L"WorkerW") || !lstrcmpW(cls, L"MontyDesktop")) return FALSE;
    if (!IsWindowVisible(fg) || IsIconic(fg)) return FALSE;

    RECT r; GetWindowRect(fg, &r);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    BOOL full = r.left <= 0 && r.top <= 0 && r.right >= sw && r.bottom >= sh;
    BOOL maxi = IsZoomed(fg) != 0;
    int mode = g_cfg.tb_applies_to;                          /* 0 fullscreen, 1 maximized, 2 both */
    BOOL hit = (mode == 0) ? full : (mode == 1) ? (maxi && !full) : (full || maxi);
    if (hit && app) *app = fg;
    return hit;
}

/* ---------------- bar window ---------------- */
static void BarRects(RECT *whole, RECT *h, RECT *v, BOOL *hasV)
{
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int bh = g_cfg.rv_height < 1 ? 1 : g_cfg.rv_height, bw = g_cfg.rv_width < 20 ? 20 : g_cfg.rv_width;
    if (bw > sw) bw = sw;
    BOOL top = g_cfg.rv_pos == 1;
    int x = g_cfg.rv_align == 0 ? 0 : g_cfg.rv_align == 2 ? sw - bw : (sw - bw) / 2;   /* left / middle / right */
    *hasV = g_cfg.rv_corner && g_cfg.rv_align != 1;                                   /* L shape needs a corner */
    int vlen = *hasV ? (bw / 3 > bh * 4 ? bw / 3 : bh * 4) : bh;
    int y = top ? 0 : sh - vlen;
    *whole = (RECT){ x, y, x + bw, y + vlen };
    int hy = top ? 0 : vlen - bh;                                                     /* local coords */
    *h = (RECT){ 0, hy, bw, hy + bh };
    if (*hasV) {
        int vx = g_cfg.rv_align == 0 ? 0 : bw - bh;
        *v = (RECT){ vx, 0, vx + bh, vlen };
    }
}

static void Place(void)
{
    RECT w, hr, vr; BOOL hasV; BarRects(&w, &hr, &vr, &hasV);
    SetWindowPos(g_hReveal, HWND_TOPMOST, w.left, w.top, w.right - w.left, w.bottom - w.top, SWP_NOACTIVATE);
    HRGN rg = CreateRectRgn(hr.left, hr.top, hr.right, hr.bottom);
    if (hasV) { HRGN v = CreateRectRgn(vr.left, vr.top, vr.right, vr.bottom); CombineRgn(rg, rg, v, RGN_OR); DeleteObject(v); }
    SetWindowRgn(g_hReveal, rg, TRUE);
    Win_SetAlpha(g_hReveal, g_cfg.rv_opacity);
    InvalidateRect(g_hReveal, NULL, FALSE);
}

static void ShowBar(BOOL show)
{
    if (show == g_barShown) return;
    g_barShown = show;
    if (show) { Place(); ShowWindow(g_hReveal, SW_SHOWNA); } else ShowWindow(g_hReveal, SW_HIDE);
}

static LRESULT CALLBACK RvProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        Palette p; Theme_Resolve(&p);
        RECT rc; GetClientRect(h, &rc);
        Draw_Fill(dc, &rc, g_cfg.rv_color == -1 ? p.accent : (COLORREF)g_cfg.rv_color);
        EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: Reveal_Tick(); return 0;
    case WM_TIMER: if (wp == RV_TIMER) Reveal_Tick(); return 0;
    case WM_APP_REFRESH: Reveal_Refresh(); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* ---------------- main logic ---------------- */
static void ApplyFsPolicy(BOOL fs)
{
    switch (g_cfg.tb_fs_mode) {
    case 1:  /* visible over app: stays topmost, nothing to do */ break;
    case 2:  Taskbar_SetFloating(fs); break;                 /* collapse to a floating start icon */
    case 3:  Taskbar_Show(!fs); break;                       /* hide bar completely */
    default: break;                                          /* 0: keep visible, normal */
    }
}

void Reveal_Tick(void)
{
    wchar_t exe[MAX_PATH] = L"";
    HWND app = NULL;
    BOOL fs = Reveal_IsFullscreenApp(&app);
    if (fs && app) { Shell_GetExeName(app, exe, MAX_PATH); if (Reveal_IsException(exe)) fs = FALSE; }   /* exception apps: behave as normal */

    if (fs != g_fsNow) {                                     /* state change: apply the visibility policy once */
        g_fsNow = fs; g_fsWnd = app; g_peeking = FALSE;
        ApplyFsPolicy(fs);
    }

    /* normal mode with "also in normal mode": the taskbar auto-hides and the reveal bar takes over */
    if (g_cfg.rv_enabled && g_cfg.rv_in_normal && !g_fsNow && !g_peeking && !Taskbar_IsHidden()) Taskbar_Show(FALSE);
    /* the bar only exists while the taskbar is away (hidden, or collapsed to its floating icon) */
    BOOL tbGone = Taskbar_IsHidden() || (g_fsNow && g_cfg.tb_fs_mode == 2 && !g_peeking);
    BOOL want = g_cfg.rv_enabled && tbGone;
    ShowBar(want && !g_peeking);

    POINT cp; GetCursorPos(&cp);
    RECT w, hr, vr; BOOL hasV; BarRects(&w, &hr, &vr, &hasV);
    BOOL over = FALSE;
    if (PtInRect(&w, cp)) {
        POINT l = { cp.x - w.left, cp.y - w.top };
        over = PtInRect(&hr, l) || (hasV && PtInRect(&vr, l));
    }
    if (want && over && !g_peeking && tbGone) {              /* hover: slide the taskbar over the app */
        g_peeking = TRUE;
        g_peekEnd = GetTickCount() + (DWORD)(g_cfg.rv_seconds < 1 ? 1 : g_cfg.rv_seconds) * 1000;
        if (g_cfg.tb_fs_mode == 2) Taskbar_SetFloating(FALSE);
        Taskbar_Peek();
        ShowBar(FALSE);
    }
    if (g_peeking) {
        RECT tb; Taskbar_GetBarRect(&tb);
        InflateRect(&tb, 4, 4);
        if (PtInRect(&tb, cp) || Preview_IsVisible()) g_peekEnd = GetTickCount() + (DWORD)(g_cfg.rv_seconds < 1 ? 1 : g_cfg.rv_seconds) * 1000;
        if ((int)(GetTickCount() - g_peekEnd) >= 0) {        /* time is up: collapse, bar comes back */
            g_peeking = FALSE;
            if (g_cfg.tb_fs_mode == 2 && g_fsNow) Taskbar_SetFloating(TRUE);
            else Taskbar_Show(FALSE);
            StartPanel_Hide();
        }
    }
}

void Reveal_Refresh(void)
{
    g_barShown = FALSE; g_fsNow = FALSE; g_peeking = FALSE;
    if (!g_cfg.rv_enabled || (!g_cfg.rv_in_normal && !g_fsNow)) ShowWindow(g_hReveal, SW_HIDE);
    if (!g_cfg.rv_in_normal && Taskbar_IsHidden() && !g_fsNow) Taskbar_Show(TRUE);
    KillTimer(g_hReveal, RV_TIMER);
    SetTimer(g_hReveal, RV_TIMER, 250, NULL);
    Reveal_Tick();
}

BOOL Reveal_Create(void)
{
    WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = RvProc; wc.hInstance = g_hInst; wc.lpszClassName = L"MontyReveal"; wc.hCursor = LoadCursorW(NULL, IDC_HAND);
    if (!RegisterClassW(&wc)) return FALSE;
    g_hReveal = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED, wc.lpszClassName, L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, g_hInst, NULL);
    if (!g_hReveal) return FALSE;
    SetTimer(g_hReveal, RV_TIMER, 250, NULL);
    return TRUE;
}
