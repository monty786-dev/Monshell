/* taskbar.c - the taskbar window: layout, painting, running-window buttons, tray, clock. */
#include "app.h"
#include <shellapi.h>
#include <iphlpapi.h>
#include <string.h>
#include <wchar.h>

#define T_POLL   1
#define T_CLOCK  2
#define T_HOVER  3
#define MAXWIN   96
#define MAXITEM  256

enum { K_START, K_PIN, K_WIN, K_DIV, K_TRAYAPP, K_TRAYGRID, K_BATT, K_WIFI, K_BT, K_TASKMGR, K_CLOCK, K_DESK };

typedef struct { int kind, idx; RECT r; } Item;
typedef struct { HWND w; wchar_t title[96]; wchar_t exe[MAX_PATH]; HICON ic; int pin; COLORREF col; } WinEnt;

HWND g_hTaskbar;
static Item   g_items[MAXITEM]; static int g_nItems;
static WinEnt g_wins[MAXWIN];   static int g_nWins;
static wchar_t g_pinExe[LIST_MAX][MAX_PATH];        /* resolved targets of pinned shortcuts */
static Palette g_pal;
static HWND   g_fg;
static unsigned g_sig;
static int    g_hot = -1, g_down = -1, g_hoverItem = -1;
static BOOL   g_hidden, g_floating, g_btOn = TRUE, g_wifiUp, g_tracking, g_showDesk;
static int    g_L, g_T, g_scrW, g_scrH;
static BOOL   g_vert, g_mirror;
static DWORD  g_peekUntil;
static RECT   g_work, g_lastWork;
static HWND   g_deskSaved[MAXWIN]; static int g_deskN;

/* ------------------------------------------------------------------ helpers */
static RECT Map(int a0, int a1, int b0, int b1)
{
    RECT r;
    if (g_mirror) { int t = g_L - a1; a1 = g_L - a0; a0 = t; }
    if (!g_vert) { r.left = a0; r.right = a1; r.top = b0; r.bottom = b1; }
    else         { r.top = a0; r.bottom = a1; r.left = b0; r.right = b1; }
    return r;
}

int Taskbar_Thickness(void) { return g_T; }
BOOL Taskbar_IsHidden(void) { return g_hidden; }

static COLORREF DominantCached(HICON ic)
{
    static struct { HICON i; COLORREF c; } cache[128]; static int n;
    for (int k = 0; k < n; k++) if (cache[k].i == ic) return cache[k].c;
    COLORREF c = Icon_Dominant(ic, g_pal.accent);
    if (n < 128) { cache[n].i = ic; cache[n].c = c; n++; }
    return c;
}

static void ResolvePins(void)
{
    for (int i = 0; i < AppList_Count(LIST_TASKBAR); i++)
        Shell_ResolveTarget(AppList_Get(LIST_TASKBAR, i), g_pinExe[i]);
}

static BOOL CALLBACK EnumProc(HWND w, LPARAM lp)
{
    (void)lp;
    if (g_nWins >= MAXWIN) return FALSE;
    if (!Shell_IsWindowAppWindow(w)) return TRUE;
    WinEnt *e = &g_wins[g_nWins];
    e->w = w;
    GetWindowTextW(w, e->title, 96);
    Shell_GetExeName(w, e->exe, MAX_PATH);
    e->ic = Icon_ForWindow(w);
    e->pin = -1;
    for (int i = 0; i < AppList_Count(LIST_TASKBAR); i++)
        if (e->exe[0] && !lstrcmpiW(g_pinExe[i], e->exe)) { e->pin = i; break; }
    e->col = g_cfg.tb_underline_adapt ? DominantCached(e->ic) : g_pal.accent;
    g_nWins++;
    return TRUE;
}

static void UpdateNet(void)
{
    ULONG len = 0; BOOL up = FALSE;
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                             NULL, NULL, &len) == ERROR_BUFFER_OVERFLOW && len) {
        IP_ADAPTER_ADDRESSES *a = (IP_ADAPTER_ADDRESSES *)HeapAlloc(GetProcessHeap(), 0, len), *p;
        if (a && GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                      NULL, a, &len) == NO_ERROR)
            for (p = a; p; p = p->Next)
                if (p->OperStatus == IfOperStatusUp && p->IfType != IF_TYPE_SOFTWARE_LOOPBACK && p->IfType != IF_TYPE_TUNNEL) up = TRUE;
        if (a) HeapFree(GetProcessHeap(), 0, a);
    }
    g_wifiUp = up;
}

/* ------------------------------------------------------------------ layout */
static int TrayIconW(void) { return UiScale(g_cfg.tb_iconsize) + 10; }

static void AddItem(int kind, int idx, RECT r)
{
    if (g_nItems < MAXITEM) { g_items[g_nItems].kind = kind; g_items[g_nItems].idx = idx; g_items[g_nItems].r = r; g_nItems++; }
}

/* Builds items in "logical" coordinates (a along the bar, b across it).
 * Pass 1 measures; if autowidth is on, g_L shrinks to content; pass 2 places. */
static int BuildItems(BOOL measureOnly)
{
    int T = g_T, m = UiScale(4), icon = UiScale(g_cfg.tb_iconsize);
    int a, nPin = AppList_Count(LIST_TASKBAR);
    BOOL titles = g_cfg.tb_showtitle && !g_vert;
    g_nItems = 0;

    if (g_floating) { AddItem(K_START, 0, Map(0, T, 0, T)); return T; }

    /* ---- right-hand (tray) group, measured first ---- */
    int bw = m;
    int deskW = g_cfg.tb_show_desktop ? UiScale(g_pal.classic ? 8 : 12) : 0;
    int clockW = 0;
    if (g_cfg.tb_clock) clockW = g_vert ? T : UiScale(g_cfg.tb_clock_day ? 112 : 84);
    int trayN = 0;
    int nTrayApps = AppList_Count(LIST_TRAY);
    bw += deskW + clockW;
    int battW = UiScale(g_vert ? 0 : 58) + (g_vert ? T : 0);
    if (g_cfg.tb_tray_taskmgr) { bw += TrayIconW(); trayN++; }
    if (g_cfg.tb_tray_bt)      { bw += TrayIconW(); trayN++; }
    if (g_cfg.tb_tray_wifi)    { bw += TrayIconW(); trayN++; }
    if (g_cfg.tb_tray_battery) { bw += battW;       trayN++; }
    if (g_cfg.tb_tray_grid)    { if (nTrayApps) bw += TrayIconW(); }
    else                       bw += nTrayApps * TrayIconW();

    /* ---- left group widths ---- */
    int startW = (g_pal.shape == SHAPE_TEXT98) ? UiScale(66) : (g_pal.shape == SHAPE_TEXTXP) ? UiScale(100) : T;
    if (g_vert) startW = T;
    int pinW = T;
    int divW = g_cfg.tb_divider ? (g_cfg.tb_divider_w == 0 ? 1 : g_cfg.tb_divider_w == 1 ? 2 : 4) + m * 2 : 0;
    int fixed = m + startW + nPin * pinW + (nPin && g_nWins ? divW : 0);
    int maxBtn = titles ? UiScale(168) : T;
    int avail = g_L - bw - fixed;
    int wbtn = T;
    /* windows that belong to a pin share the pin's button */
    int nFree = 0; for (int i = 0; i < g_nWins; i++) if (g_wins[i].pin < 0) nFree++;
    if (nFree) {
        wbtn = titles ? avail / nFree : T;
        if (wbtn > maxBtn) wbtn = maxBtn;
        if (wbtn < T) wbtn = T;
    }
    int aw = fixed + nFree * wbtn;                       /* width of the left group */

    if (measureOnly) return aw + bw + m;

    int x0 = 0;                                          /* start of the left group */
    if (g_cfg.tb_align == 1 && !g_cfg.tb_autowidth && !g_vert) {
        x0 = (g_L - bw - aw) / 2; if (x0 < 0) x0 = 0;    /* centred icons (Win11 / dock style) */
    }
    int pad = 2;
    a = x0 + m;
    AddItem(K_START, 0, Map(a, a + startW, 0, T)); a += startW;
    for (int i = 0; i < nPin; i++) { AddItem(K_PIN, i, Map(a, a + pinW, pad, T - pad)); a += pinW; }
    if (nPin && nFree && g_cfg.tb_divider) { AddItem(K_DIV, 0, Map(a + m, a + divW - m, T / 5, T - T / 5)); a += divW; }
    for (int i = 0; i < g_nWins; i++)
        if (g_wins[i].pin < 0) { AddItem(K_WIN, i, Map(a, a + wbtn, pad, T - pad)); a += wbtn; }

    /* right group laid out from the far end inwards */
    int e = g_L - m;
    if (deskW)  { AddItem(K_DESK,  0, Map(e - deskW, e, 0, T)); e -= deskW; }
    if (clockW) { AddItem(K_CLOCK, 0, Map(e - clockW, e, 0, T)); e -= clockW; }
    int tw = TrayIconW();
    if (g_cfg.tb_tray_battery) { AddItem(K_BATT, 0, Map(e - battW, e, 0, T)); e -= battW; }
    if (g_cfg.tb_tray_wifi)    { AddItem(K_WIFI, 0, Map(e - tw, e, 0, T)); e -= tw; }
    if (g_cfg.tb_tray_bt)      { AddItem(K_BT,   0, Map(e - tw, e, 0, T)); e -= tw; }
    if (g_cfg.tb_tray_taskmgr) { AddItem(K_TASKMGR, 0, Map(e - tw, e, 0, T)); e -= tw; }
    if (g_cfg.tb_tray_grid) { if (nTrayApps) { AddItem(K_TRAYGRID, 0, Map(e - tw, e, 0, T)); e -= tw; } }
    else for (int i = nTrayApps - 1; i >= 0; i--) { AddItem(K_TRAYAPP, i, Map(e - tw, e, 0, T)); e -= tw; }
    (void)icon;
    return aw + bw + m;
}

static void ApplyWorkArea(void)
{
    RECT wa = { 0, 0, g_scrW, g_scrH };
    if (!g_hidden && !g_floating) {
        int t = g_T + g_cfg.tb_detach;
        switch (g_cfg.tb_position) {
        case 0: wa.bottom -= t; break;  case 1: wa.top += t; break;
        case 2: wa.left += t;   break;  default: wa.right -= t; break;
        }
    }
    if (!EqualRect(&wa, &g_lastWork)) { g_lastWork = wa; SystemParametersInfoW(SPI_SETWORKAREA, 0, &wa, SPIF_SENDCHANGE); }
}

void Taskbar_Relayout(void)
{
    if (!g_hTaskbar) return;
    Theme_Resolve(&g_pal);
    g_scrW = GetSystemMetrics(SM_CXSCREEN); g_scrH = GetSystemMetrics(SM_CYSCREEN);
    g_vert   = g_cfg.tb_position >= 2;
    g_mirror = !g_vert && g_cfg.tb_align == 2;             /* right alignment: start button in the right corner */
    g_T = g_cfg.tb_height > 0 ? UiScale(g_cfg.tb_height) : UiScale(g_pal.height);
    if (g_T < 20) g_T = 20;
    int maxL = g_vert ? g_scrH : g_scrW;
    int d = g_cfg.tb_detach;

    g_L = maxL - 2 * (d ? d : 0);
    if (g_L < 100) g_L = 100;
    if (g_cfg.tb_autowidth && !g_vert && !g_floating) {
        int need = BuildItems(TRUE);
        if (need < g_L) g_L = need < g_T * 3 ? g_T * 3 : need;
    }
    BuildItems(FALSE);

    int len = g_floating ? g_T : g_L, thick = g_T;
    int x, y, w, h;
    int off = (maxL - len) / 2;                           /* centred along the edge when shorter than the screen */
    if (g_cfg.tb_align == 0 && g_cfg.tb_autowidth) off = d;
    if (g_cfg.tb_align == 2 && g_cfg.tb_autowidth) off = maxL - len - d;
    if (g_floating) off = d + 8;
    switch (g_cfg.tb_position) {
    case 0: x = off; y = g_scrH - thick - d - (g_floating ? 8 : 0); w = len; h = thick; break;
    case 1: x = off; y = d + (g_floating ? 8 : 0);                  w = len; h = thick; break;
    case 2: x = d + (g_floating ? 8 : 0); y = off;                  w = thick; h = len; break;
    default:x = g_scrW - thick - d - (g_floating ? 8 : 0); y = off; w = thick; h = len; break;
    }
    HWND after = g_cfg.tb_zorder == 0 ? HWND_TOPMOST : HWND_BOTTOM;
    SetWindowPos(g_hTaskbar, after, x, y, w, h, SWP_NOACTIVATE | (g_hidden ? 0 : SWP_SHOWWINDOW));
    int rad = g_floating ? g_T / 2 : ((g_cfg.tb_rounded || d) ? (g_pal.radius ? g_pal.radius : 10) : 0);
    Win_SetRound(g_hTaskbar, rad);
    ApplyWorkArea();
    InvalidateRect(g_hTaskbar, NULL, FALSE);
}

void Taskbar_Refresh(void)
{
    ResolvePins();
    g_sig = 0;
    Taskbar_Relayout();
    SetTimer(g_hTaskbar, T_POLL, g_cfg.perf_poll_ms < 150 ? 150 : g_cfg.perf_poll_ms, NULL);
}

void Taskbar_GetBarRect(RECT *r) { GetWindowRect(g_hTaskbar, r); }

void Taskbar_Show(BOOL show)
{
    if (!g_hTaskbar) return;
    g_hidden = !show;
    if (show) { SetWindowPos(g_hTaskbar, g_cfg.tb_zorder == 0 ? HWND_TOPMOST : HWND_BOTTOM, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW); }
    else      { ShowWindow(g_hTaskbar, SW_HIDE); Preview_Hide(); TrayGrid_Hide(); }
    ApplyWorkArea();
}

void Taskbar_SetFloating(BOOL f)
{
    if (f == g_floating) return;
    g_floating = f;
    Taskbar_Relayout();
}

void Taskbar_Peek(void)
{
    int secs = g_cfg.rv_seconds < 1 ? 1 : g_cfg.rv_seconds;
    g_peekUntil = GetTickCount() + (DWORD)secs * 1000;
    if (IsWindowVisible(g_hTaskbar)) return;
    g_hidden = FALSE;
    Taskbar_Relayout();
    ShowWindow(g_hTaskbar, SW_HIDE);
    DWORD fl;
    int pos = g_cfg.tb_position;
    switch (g_cfg.rv_anim) {
    case 1:  fl = AW_CENTER; break;                                              /* pop-up */
    case 3:  fl = AW_SLIDE | AW_HOR_POSITIVE; break;                             /* from left */
    case 4:  fl = AW_SLIDE | AW_HOR_NEGATIVE; break;                             /* from right */
    case 5:  fl = AW_BLEND; break;                                               /* fade */
    case 0:                                                                      /* blink */
        ShowWindow(g_hTaskbar, SW_SHOWNA); Sleep(40); ShowWindow(g_hTaskbar, SW_HIDE); Sleep(40);
        ShowWindow(g_hTaskbar, SW_SHOWNA); goto done;
    default: fl = AW_SLIDE | (pos == 1 ? AW_VER_POSITIVE : pos == 2 ? AW_HOR_POSITIVE : pos == 3 ? AW_HOR_NEGATIVE : AW_VER_NEGATIVE);
    }
    if (!AnimateWindow(g_hTaskbar, 160, fl)) ShowWindow(g_hTaskbar, SW_SHOWNA);
done:
    SetWindowPos(g_hTaskbar, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    InvalidateRect(g_hTaskbar, NULL, FALSE);
}

/* ------------------------------------------------------------------ painting */
static void FormatClock(wchar_t *t, int tn, wchar_t *d, int dn)
{
    SYSTEMTIME st; GetLocalTime(&st);
    GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, t, tn);
    if (g_cfg.tb_clock_day) {
        wchar_t day[16]; GetDateFormatW(LOCALE_USER_DEFAULT, 0, &st, L"ddd", day, 16);
        wchar_t dt[32];  GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, dt, 32);
        wsprintfW(d, L"%s  %s", day, dt);
    } else GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, d, dn);
}

static BOOL AppActive(int winIdx) { return g_wins[winIdx].w == g_fg; }
static BOOL PinActive(int pin) { for (int i = 0; i < g_nWins; i++) if (g_wins[i].pin == pin && AppActive(i)) return TRUE; return FALSE; }
static BOOL PinRunning(int pin) { for (int i = 0; i < g_nWins; i++) if (g_wins[i].pin == pin) return TRUE; return FALSE; }

static void DrawUnderline(HDC dc, const RECT *r, COLORREF c, BOOL active)
{
    if (!g_cfg.tb_underline) return;
    int h = g_cfg.tb_underline_h < 1 ? 1 : g_cfg.tb_underline_h;
    int ins = active ? 3 : UiScale(10);                 /* active app gets the longer line */
    RECT u = *r;
    switch (g_cfg.tb_position) {                         /* line sits on the edge facing the screen border */
    case 0:  u.top = r->bottom - h; u.left += ins; u.right -= ins; break;
    case 1:  u.bottom = r->top + h; u.left += ins; u.right -= ins; break;
    case 2:  u.right = r->left + h; u.top += ins;  u.bottom -= ins; break;
    default: u.left = r->right - h; u.top += ins;  u.bottom -= ins; break;
    }
    Draw_Round(dc, &u, c, CLR_NONE, h > 2 ? h / 2 : 0);
}

static void DrawButtonBg(HDC dc, const RECT *r, BOOL active, BOOL hot, BOOL down)
{
    if (g_pal.classic && g_pal.bevel) {                  /* Win9x: buttons are always bevelled */
        Draw_Fill(dc, r, active ? Col_Blend(g_pal.bg, RGB(255,255,255), 55) : g_pal.bg);
        Draw_Bevel(dc, r, !(active || down));
        return;
    }
    int rad = g_pal.radius > 1 ? g_pal.radius / 2 : 0;
    if (active)            Draw_Round(dc, r, Col_Blend(g_pal.bg, g_pal.fg, hot ? 26 : 18), CLR_NONE, rad);
    else if (hot || down)  Draw_Round(dc, r, g_pal.hover, CLR_NONE, rad);
}

static void DrawTray(HDC dc, const Item *it, int glyph, COLORREF col, BOOL hot)
{
    if (hot) Draw_Round(dc, &it->r, g_pal.hover, CLR_NONE, g_pal.radius / 2);
    Draw_Glyph(dc, &it->r, glyph, col, g_cfg.p_iconpreset);
}

static void Paint(HDC wdc)
{
    RECT rc; GetClientRect(g_hTaskbar, &rc);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bm = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(dc, bm);

    Draw_Surface(dc, &rc, &g_pal, g_pal.bg, g_cfg.p_fx_taskbar);
    if (g_pal.classic && g_pal.bevel) { RECT l = { 0, 0, rc.right, 1 }; Draw_Fill(dc, &l, RGB(223,223,223)); }

    int icon = UiScale(g_cfg.tb_iconsize);
    HFONT f = Theme_Font(g_cfg.p_fontsize, FALSE);
    BOOL titles = g_cfg.tb_showtitle && !g_vert;
    SYSTEM_POWER_STATUS sp; BOOL hasPwr = GetSystemPowerStatus(&sp);

    for (int n = 0; n < g_nItems; n++) {
        const Item *it = &g_items[n];
        BOOL hot = (n == g_hot), down = (n == g_down && hot);
        int cx = (it->r.left + it->r.right) / 2, cy = (it->r.top + it->r.bottom) / 2;
        switch (it->kind) {
        case K_START:
            Draw_StartButton(dc, &it->r, &g_pal, hot, down || (g_hStart && IsWindowVisible(g_hStart)));
            break;
        case K_PIN: {
            const wchar_t *p = AppList_Get(LIST_TASKBAR, it->idx);
            BOOL run = PinRunning(it->idx), act = PinActive(it->idx);
            DrawButtonBg(dc, &it->r, act, hot, down);
            Draw_Icon(dc, cx - icon / 2, cy - icon / 2, icon, Icon_ForPath(p, icon > 20), &g_pal);
            if (run) { COLORREF c = g_pal.accent;
                for (int i = 0; i < g_nWins; i++) if (g_wins[i].pin == it->idx) { c = g_wins[i].col; break; }
                DrawUnderline(dc, &it->r, c, act); }
            break; }
        case K_WIN: {
            const WinEnt *e = &g_wins[it->idx];
            BOOL act = (e->w == g_fg);
            DrawButtonBg(dc, &it->r, act, hot, down);
            int ix = titles ? it->r.left + UiScale(8) : cx - icon / 2;
            Draw_Icon(dc, ix, cy - icon / 2, icon, e->ic, &g_pal);
            if (titles) {
                RECT t = it->r; t.left = ix + icon + UiScale(6); t.right -= UiScale(6);
                Draw_Text(dc, &t, e->title, g_pal.fg, f, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            }
            DrawUnderline(dc, &it->r, e->col, act);
            break; }
        case K_DIV: {
            int w = g_cfg.tb_divider_w == 0 ? 1 : g_cfg.tb_divider_w == 1 ? 2 : 4;
            RECT l = it->r;
            if (!g_vert) { l.left = cx - w / 2; l.right = l.left + w; } else { l.top = cy - w / 2; l.bottom = l.top + w; }
            Draw_Fill(dc, &l, g_pal.fgDim);
            break; }
        case K_TRAYAPP:
            if (hot) Draw_Round(dc, &it->r, g_pal.hover, CLR_NONE, g_pal.radius / 2);
            Draw_Icon(dc, cx - 8, cy - 8, 16, Icon_ForPath(AppList_Get(LIST_TRAY, it->idx), FALSE), &g_pal);
            break;
        case K_TRAYGRID: DrawTray(dc, it, G_GRID, g_pal.fg, hot); break;
        case K_TASKMGR:  DrawTray(dc, it, G_TASKMGR, g_pal.fg, hot); break;
        case K_BT:       DrawTray(dc, it, G_BT, g_btOn ? g_pal.accent : g_pal.fgDim, hot); break;
        case K_WIFI:
            DrawTray(dc, it, G_WIFI, g_wifiUp ? g_pal.fg : g_pal.fgDim, hot);
            if (!g_wifiUp) { HPEN p = CreatePen(PS_SOLID, 1, g_pal.fgDim); HGDIOBJ o = SelectObject(dc, p);
                MoveToEx(dc, cx - 7, cy + 7, NULL); LineTo(dc, cx + 8, cy - 8); SelectObject(dc, o); DeleteObject(p); }
            break;
        case K_BATT: {
            int pct = hasPwr && sp.BatteryLifePercent <= 100 ? sp.BatteryLifePercent : -1;
            COLORREF c = g_pal.fg;
            if (pct >= 0 && pct <= 20 && sp.ACLineStatus != 1) c = RGB(232,64,64);       /* low battery */
            else if (hasPwr && sp.ACLineStatus == 1) c = RGB(80,200,110);                /* charging / AC */
            RECT gl = it->r; if (!g_vert) gl.right = gl.left + TrayIconW();
            if (hot) Draw_Round(dc, &it->r, g_pal.hover, CLR_NONE, g_pal.radius / 2);
            Draw_Glyph(dc, &gl, G_BATTERY, c, g_cfg.p_iconpreset);
            if (pct >= 0) {
                int u = (gl.right - gl.left) / 2 - 4; if (u < 3) u = 3;
                int gx = (gl.left + gl.right) / 2, gy = (gl.top + gl.bottom) / 2;
                RECT fill = { gx - u + 1, gy - u / 2 + 1, gx - u + 1 + (2 * u - 2) * pct / 100, gy + u / 2 };
                Draw_Fill(dc, &fill, c);
            }
            if (!g_vert) {
                wchar_t s[12]; if (pct >= 0) wsprintfW(s, L"%d%%", pct); else lstrcpyW(s, L"AC");
                RECT t = it->r; t.left = gl.right - 2;
                Draw_Text(dc, &t, s, g_pal.fg, Theme_Font(g_cfg.p_fontsize > 1 ? g_cfg.p_fontsize - 1 : 1, FALSE), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            }
            break; }
        case K_CLOCK: {
            wchar_t t[32], d[48]; FormatClock(t, 32, d, 48);
            if (hot) Draw_Round(dc, &it->r, g_pal.hover, CLR_NONE, g_pal.radius / 2);
            if (g_T >= UiScale(34) || g_vert) {
                RECT a = it->r, b = it->r; a.bottom = cy; b.top = cy;
                Draw_Text(dc, &a, t, g_pal.fg, f, DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
                Draw_Text(dc, &b, d, g_pal.fgDim, Theme_Font(g_cfg.p_fontsize > 1 ? g_cfg.p_fontsize - 1 : 1, FALSE), DT_SINGLELINE | DT_CENTER | DT_TOP);
            } else Draw_Text(dc, &it->r, t, g_pal.fg, f, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            break; }
        case K_DESK: {
            RECT s = it->r;
            if (g_pal.classic && g_pal.bevel) Draw_Bevel(dc, &s, TRUE);
            else {
                if (hot) Draw_Fill(dc, &s, g_pal.hover);
                RECT l = s; if (!g_vert) l.right = l.left + 1; else l.bottom = l.top + 1;
                Draw_Fill(dc, &l, g_pal.fgDim);
            }
            break; }
        }
    }
    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob); DeleteObject(bm); DeleteDC(dc);
}

/* ------------------------------------------------------------------ polling */
static int g_curInterval, g_pollCount;

static void Poll(void)
{
    HWND oldFg = g_fg;
    g_fg = GetForegroundWindow();
    g_nWins = 0;
    EnumWindows(EnumProc, 0);

    unsigned sig = (unsigned)g_nWins * 131u;
    for (int i = 0; i < g_nWins; i++) {
        sig = sig * 31u + (unsigned)(UINT_PTR)g_wins[i].w;
        for (const wchar_t *c = g_wins[i].title; *c && g_cfg.tb_showtitle; c++) sig = sig * 33u + *c;
        sig = sig * 7u + (IsIconic(g_wins[i].w) ? 3u : 1u) + (g_wins[i].w == g_fg ? 5u : 0u);
    }
    if (sig != g_sig || oldFg != g_fg) {                 /* repaint only when something actually changed */
        g_sig = sig;
        if (g_cfg.tb_autowidth) Taskbar_Relayout();
        else { BuildItems(FALSE); InvalidateRect(g_hTaskbar, NULL, FALSE); }
    }
    if (!g_hidden && g_cfg.tb_zorder == 0 && !(GetWindowLongW(g_hTaskbar, GWL_EXSTYLE) & WS_EX_TOPMOST))
        SetWindowPos(g_hTaskbar, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    g_pollCount++;
    if (g_cfg.tb_hide_native && g_pollCount % 6 == 1) Shell_HideNativeTaskbar(TRUE);
    if (g_pollCount % 10 == 1) UpdateNet();

    /* power saving: poll half as often on battery */
    int base = g_cfg.perf_poll_ms < 150 ? 150 : g_cfg.perf_poll_ms;
    int want = (g_cfg.perf_battery_lite && Power_OnBattery()) ? base * 2 : base;
    if (want != g_curInterval) { g_curInterval = want; SetTimer(g_hTaskbar, T_POLL, want, NULL); }
}

/* ------------------------------------------------------------------ actions */
static void ToggleWindow(HWND w)
{
    if (IsIconic(w))                 { ShowWindow(w, SW_RESTORE); SetForegroundWindow(w); }
    else if (w == GetForegroundWindow()) ShowWindow(w, SW_MINIMIZE);
    else                             { SetForegroundWindow(w); BringWindowToTop(w); }
}

static void ShowDesktopToggle(void)
{
    if (!g_showDesk) {
        g_deskN = 0;
        for (int i = 0; i < g_nWins && g_deskN < MAXWIN; i++)
            if (!IsIconic(g_wins[i].w)) { g_deskSaved[g_deskN++] = g_wins[i].w; ShowWindow(g_wins[i].w, SW_MINIMIZE); }
        g_showDesk = g_deskN > 0;
    } else {
        for (int i = g_deskN - 1; i >= 0; i--) if (IsWindow(g_deskSaved[i])) ShowWindow(g_deskSaved[i], SW_RESTORE);
        g_showDesk = FALSE;
    }
}

static int HitTest(int x, int y)
{
    POINT p = { x, y };
    for (int i = g_nItems - 1; i >= 0; i--) if (g_items[i].kind != K_DIV && PtInRect(&g_items[i].r, p)) return i;
    return -1;
}

static void ItemScreenRect(int n, RECT *out)
{
    RECT w; GetWindowRect(g_hTaskbar, &w);
    *out = g_items[n].r; OffsetRect(out, w.left, w.top);
}

static void ClickItem(int n)
{
    const Item *it = &g_items[n];
    switch (it->kind) {
    case K_START:   StartPanel_Toggle(); break;
    case K_PIN: {
        HWND target = NULL;
        for (int i = 0; i < g_nWins; i++) if (g_wins[i].pin == it->idx) { if (!target || g_wins[i].w != g_fg) target = g_wins[i].w; }
        if (target) ToggleWindow(target); else Shell_Run(AppList_Get(LIST_TASKBAR, it->idx));
        break; }
    case K_WIN:     ToggleWindow(g_wins[it->idx].w); break;
    case K_TRAYAPP: Shell_Run(AppList_Get(LIST_TRAY, it->idx)); break;
    case K_TRAYGRID: { RECT r; ItemScreenRect(n, &r); TrayGrid_Toggle(&r); break; }
    case K_BT:      g_btOn = !g_btOn; InvalidateRect(g_hTaskbar, NULL, FALSE); break;   /* indicator only: Wine cannot reach Android radios */
    case K_TASKMGR: Shell_Run(L"taskmgr.exe"); break;
    case K_DESK:    ShowDesktopToggle(); break;
    }
}

enum { ID_SET = 1, ID_DESK, ID_TASKMGR, ID_RESTART, ID_EXIT, ID_UNPIN, ID_PIN, ID_CLOSE, ID_OPEN };

static void ContextMenu(HWND hwnd, int x, int y)
{
    int n = HitTest(x, y);
    HMENU m = CreatePopupMenu();
    int kind = n >= 0 ? g_items[n].kind : -1;
    if (kind == K_PIN) {
        AppendMenuW(m, MF_STRING, ID_OPEN, L"Open");
        AppendMenuW(m, MF_STRING, ID_UNPIN, L"Unpin from taskbar");
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    } else if (kind == K_WIN) {
        AppendMenuW(m, MF_STRING, ID_PIN, L"Pin to taskbar");
        AppendMenuW(m, MF_STRING, ID_CLOSE, L"Close window");
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    }
    AppendMenuW(m, MF_STRING, ID_SET, L"Taskbar settings");
    AppendMenuW(m, MF_STRING, ID_DESK, L"Show desktop");
    AppendMenuW(m, MF_STRING, ID_TASKMGR, L"Task manager");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_RESTART, L"Restart shell");
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Exit shell");
    POINT pt = { x, y }; ClientToScreen(hwnd, &pt);
    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | (g_cfg.tb_position == 0 ? TPM_BOTTOMALIGN : 0), pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(m);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    switch (cmd) {
    case ID_SET: SettingsUI_Show(0); break;
    case ID_DESK: ShowDesktopToggle(); break;
    case ID_TASKMGR: Shell_Run(L"taskmgr.exe"); break;
    case ID_RESTART: Shell_PowerAction(2); break;
    case ID_EXIT: Shell_PowerAction(1); break;
    case ID_OPEN: ClickItem(n); break;
    case ID_UNPIN: AppList_Remove(LIST_TASKBAR, g_items[n].idx); Settings_Save(); Taskbar_Refresh(); break;
    case ID_PIN: if (g_wins[g_items[n].idx].exe[0]) { AppList_Add(LIST_TASKBAR, g_wins[g_items[n].idx].exe); Settings_Save(); Taskbar_Refresh(); } break;
    case ID_CLOSE: PostMessageW(g_wins[g_items[n].idx].w, WM_CLOSE, 0, 0); break;
    }
}

/* ------------------------------------------------------------------ window proc */
static LRESULT CALLBACK TbProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;            /* never steal focus from the running app */
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); Paint(dc); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: {
        int n = HitTest((short)LOWORD(lp), (short)HIWORD(lp));
        if (!g_tracking) { TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 }; TrackMouseEvent(&t); g_tracking = TRUE; }
        if (n != g_hot) {
            g_hot = n; InvalidateRect(h, NULL, FALSE);
            KillTimer(h, T_HOVER);
            if (n >= 0 && g_cfg.tb_preview && (g_items[n].kind == K_WIN || g_items[n].kind == K_PIN)) SetTimer(h, T_HOVER, 350, NULL);
        }
        return 0; }
    case WM_MOUSELEAVE: g_tracking = FALSE; g_hot = -1; KillTimer(h, T_HOVER); InvalidateRect(h, NULL, FALSE); return 0;
    case WM_LBUTTONDOWN: SetCapture(h); g_down = g_hot; InvalidateRect(h, NULL, FALSE); return 0;
    case WM_LBUTTONUP: {
        ReleaseCapture();
        int n = HitTest((short)LOWORD(lp), (short)HIWORD(lp));
        int d = g_down; g_down = -1;
        InvalidateRect(h, NULL, FALSE);
        if (n >= 0 && n == d) ClickItem(n);
        return 0; }
    case WM_MBUTTONUP: {
        int n = HitTest((short)LOWORD(lp), (short)HIWORD(lp));
        if (n >= 0 && g_items[n].kind == K_WIN) PostMessageW(g_wins[g_items[n].idx].w, WM_CLOSE, 0, 0);
        return 0; }
    case WM_RBUTTONUP: ContextMenu(h, (short)LOWORD(lp), (short)HIWORD(lp)); return 0;
    case WM_TIMER:
        if (wp == T_POLL) Poll();
        else if (wp == T_CLOCK) {
            static int lastMin = -1, lastPct = -2;
            SYSTEMTIME st; GetLocalTime(&st);
            SYSTEM_POWER_STATUS sp; int pct = GetSystemPowerStatus(&sp) ? sp.BatteryLifePercent * 4 + sp.ACLineStatus : -1;
            if (st.wMinute != lastMin || pct != lastPct) {   /* cheap: only repaint when the minute / battery changes */
                lastMin = st.wMinute; lastPct = pct;
                for (int i = 0; i < g_nItems; i++) if (g_items[i].kind == K_CLOCK || g_items[i].kind == K_BATT) InvalidateRect(h, &g_items[i].r, FALSE);
            }
        } else if (wp == T_HOVER) {
            KillTimer(h, T_HOVER);
            if (g_hot >= 0) {
                HWND target = NULL; const Item *it = &g_items[g_hot];
                if (it->kind == K_WIN) target = g_wins[it->idx].w;
                else if (it->kind == K_PIN) for (int i = 0; i < g_nWins; i++) if (g_wins[i].pin == it->idx) { target = g_wins[i].w; break; }
                if (target) { RECT r; ItemScreenRect(g_hot, &r); Preview_Show(target, &r); }
            }
        }
        return 0;
    case WM_DISPLAYCHANGE: Taskbar_Relayout(); return 0;
    case WM_APP_REFRESH: Taskbar_Refresh(); return 0;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: {
        RECT wa = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        SystemParametersInfoW(SPI_SETWORKAREA, 0, &wa, SPIF_SENDCHANGE);   /* give the screen back */
        Shell_HideNativeTaskbar(FALSE);
        PostQuitMessage(0);
        return 0; }
    }
    return DefWindowProcW(h, m, wp, lp);
}

BOOL Taskbar_Create(void)
{
    WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = TbProc; wc.hInstance = g_hInst; wc.lpszClassName = L"MontyTaskbar";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    if (!RegisterClassW(&wc)) return FALSE;
    g_hTaskbar = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, wc.lpszClassName, L"Taskbar",
                                 WS_POPUP, 0, 0, 100, 40, NULL, NULL, g_hInst, NULL);
    if (!g_hTaskbar) return FALSE;
    Theme_Resolve(&g_pal);
    ResolvePins();
    UpdateNet();
    Poll();
    Taskbar_Relayout();
    ShowWindow(g_hTaskbar, SW_SHOWNA);
    SetTimer(g_hTaskbar, T_CLOCK, 1000, NULL);
    return TRUE;
}
