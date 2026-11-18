/* desktop.c - optional desktop mode: wallpaper, themed icon grid, clock widget. */
#include "app.h"
#include <shlobj.h>
#include <string.h>
#include <stdlib.h>

#define MAXICON 160
typedef struct { wchar_t path[MAX_PATH]; wchar_t name[64]; int col, row; } DIcon;

static HWND  g_dk;
static DIcon g_ic[MAXICON]; static int g_n;
static int   g_sel = -1, g_drag = -1, g_dragDX, g_dragDY, g_dragX, g_dragY;
static RECT  g_work;

static int Cell(void)  { int c = g_cfg.dk_grid * g_cfg.dk_uiscale / 100; return c < 48 ? 48 : c; }
static int IconPx(void){ int s = Cell() / 2; return s < 24 ? 24 : (s > 48 ? 48 : s); }

static HFONT DkFont(BOOL bold, int extra)
{
    static HFONT f[2][3]; static int key = -1;
    int k = g_cfg.dk_fontsize * 1000 + g_cfg.dk_uiscale;
    if (k != key) { for (int a = 0; a < 2; a++) for (int b = 0; b < 3; b++) if (f[a][b]) { DeleteObject(f[a][b]); f[a][b] = NULL; } key = k; }
    int slot = extra < 6 ? 0 : (extra < 20 ? 1 : 2);
    if (!f[bold][slot]) {
        int pt = g_cfg.dk_fontsize + (slot == 1 ? 14 : slot == 2 ? 22 : 0);
        int px = -MulDiv(pt, 96, 72) * g_cfg.dk_uiscale / 100;
        f[bold][slot] = CreateFontW(px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, g_cfg.font_name);
    }
    return f[bold][slot];
}

/* ---------------- icon list & positions ---------------- */
static void ScanFolder(int csidl)
{
    wchar_t dir[MAX_PATH], pat[MAX_PATH];
    if (SHGetFolderPathW(NULL, csidl, NULL, 0, dir) != S_OK) return;
    wsprintfW(pat, L"%s\\*", dir);
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == L'.' || (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || g_n >= MAXICON) continue;
        if (!lstrcmpiW(fd.cFileName, L"desktop.ini")) continue;
        DIcon *d = &g_ic[g_n++];
        wsprintfW(d->path, L"%s\\%s", dir, fd.cFileName);
        Shell_DisplayName(d->path, d->name, 64);
        d->col = d->row = -1;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void Place(void)
{
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &g_work, 0);
    int c = Cell(), cols = (g_work.right - g_work.left) / c, rows = (g_work.bottom - g_work.top) / c;
    if (cols < 1) cols = 1; if (rows < 1) rows = 1;
    const wchar_t *ini = Settings_Path(); wchar_t v[32];
    BOOL used[64][64]; memset(used, 0, sizeof used);
    for (int i = 0; i < g_n; i++) g_ic[i].col = g_ic[i].row = -1;
    for (int i = 0; i < g_n; i++) {                                    /* saved positions first */
        GetPrivateProfileStringW(L"DesktopPos", g_ic[i].name, L"", v, 32, ini);
        wchar_t *ep = NULL; int a = (int)wcstol(v, &ep, 10), b = (ep && *ep == L',') ? (int)wcstol(ep + 1, NULL, 10) : -1;
        if (v[0] && a >= 0 && b >= 0 && a < cols && b < rows && a < 64 && b < 64 && !used[a][b]) { g_ic[i].col = a; g_ic[i].row = b; used[a][b] = TRUE; }
    }
    int corner = g_cfg.dk_corner;                                      /* 0 TL, 1 TR, 2 BL, 3 BR; flow column by column */
    for (int i = 0; i < g_n; i++) if (g_ic[i].col < 0) {
        for (int k = 0; k < cols * rows; k++) {
            int cc = k / rows, rr = k % rows;
            int x = (corner & 1) ? cols - 1 - cc : cc, y = (corner & 2) ? rows - 1 - rr : rr;
            if (x < 64 && y < 64 && !used[x][y]) { used[x][y] = TRUE; g_ic[i].col = x; g_ic[i].row = y; break; }
        }
    }
}

static RECT IconRect(int i)
{
    int c = Cell();
    RECT r = { g_work.left + g_ic[i].col * c, g_work.top + g_ic[i].row * c, 0, 0 };
    r.right = r.left + c; r.bottom = r.top + c;
    return r;
}

static void Rescan(void)
{
    g_n = 0; g_sel = -1;
    if (g_cfg.dk_show_icons) { ScanFolder(CSIDL_DESKTOPDIRECTORY); ScanFolder(CSIDL_COMMON_DESKTOPDIRECTORY); }
    Place();
    InvalidateRect(g_dk, NULL, FALSE);
}

static void SavePos(int i)
{
    wchar_t v[32]; wsprintfW(v, L"%d,%d", g_ic[i].col, g_ic[i].row);
    WritePrivateProfileStringW(L"DesktopPos", g_ic[i].name, v, Settings_Path());
}

/* ---------------- painting ---------------- */
static void Paint(HDC wdc)
{
    Palette p; Theme_Resolve(&p);
    RECT rc; GetClientRect(g_dk, &rc);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bm = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(dc, bm);

    COLORREF bg;
    if (g_cfg.dk_wall_color != -1) bg = (COLORREF)g_cfg.dk_wall_color;
    else if (p.classic && p.bevel) bg = RGB(0,128,128);                  /* the famous Win95/98 teal */
    else bg = Col_Blend(p.bg, p.accent, 40);
    if (g_cfg.dk_wall_color == -1 && !(p.classic && p.bevel)) Draw_Gradient(dc, &rc, Col_Blend(bg, RGB(0,0,0), 30), Col_Blend(bg, p.accent, 30), TRUE);
    else Draw_Fill(dc, &rc, bg);
    HBITMAP wp = Img_Load(g_cfg.wallpaper);
    if (wp) Img_Draw(dc, &rc, wp, g_cfg.dk_wall_fit);

    COLORREF txt = Col_Text(bg);
    int c = Cell(), is = IconPx();
    for (int i = 0; i < g_n; i++) {
        RECT r = IconRect(i);
        if (i == g_drag) OffsetRect(&r, g_dragX - r.left - g_dragDX, g_dragY - r.top - g_dragDY);
        if (i == g_sel) Draw_Alpha(dc, &r, p.accent, 90);
        Draw_Icon(dc, (r.left + r.right) / 2 - is / 2, r.top + 6, is, Icon_ForPath(g_ic[i].path, is > 24), &p);
        RECT t = { r.left + 2, r.top + is + 8, r.right - 2, r.bottom };
        RECT sh = t; OffsetRect(&sh, 1, 1);
        Draw_Text(dc, &sh, g_ic[i].name, Col_Text(txt), DkFont(FALSE, 0), DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);   /* shadow keeps text readable */
        Draw_Text(dc, &t, g_ic[i].name, txt, DkFont(FALSE, 0), DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
    }
    (void)c;
    if (g_cfg.dk_widget_clock) {                                         /* clock widget in the corner opposite the icons */
        SYSTEMTIME st; GetLocalTime(&st);
        wchar_t t[24], d[48];
        GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, t, 24);
        GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, d, 48);
        int w = 260 * g_cfg.dk_uiscale / 100, h = 110 * g_cfg.dk_uiscale / 100;
        BOOL left = (g_cfg.dk_corner & 1);
        RECT wr = { left ? g_work.left + 16 : g_work.right - w - 16, g_work.top + 16, 0, 0 };
        wr.right = wr.left + w; wr.bottom = wr.top + h;
        Draw_Surface(dc, &wr, &p, Col_Blend(p.bg, p.accent, 15), TRUE);
        RECT a = wr, b = wr; a.bottom = wr.top + h * 6 / 10; b.top = a.bottom;
        Draw_Text(dc, &a, t, p.fg, DkFont(TRUE, 30), DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        Draw_Text(dc, &b, d, p.fgDim, DkFont(FALSE, 0), DT_SINGLELINE | DT_CENTER | DT_TOP);
    }
    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob); DeleteObject(bm); DeleteDC(dc);
}

static int HitIcon(int x, int y) { POINT p = { x, y }; for (int i = g_n - 1; i >= 0; i--) { RECT r = IconRect(i); if (PtInRect(&r, p)) return i; } return -1; }

static LRESULT CALLBACK DkProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); Paint(dc); EndPaint(h, &ps); return 0; }
    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp), i = HitIcon(x, y);
        g_sel = i;
        if (i >= 0 && !g_cfg.dk_lock) { RECT r = IconRect(i); g_drag = i; g_dragDX = x - r.left; g_dragDY = y - r.top; g_dragX = x; g_dragY = y; SetCapture(h); }
        InvalidateRect(h, NULL, FALSE); return 0; }
    case WM_MOUSEMOVE: if (g_drag >= 0 && (wp & MK_LBUTTON)) { g_dragX = (short)LOWORD(lp); g_dragY = (short)HIWORD(lp); InvalidateRect(h, NULL, FALSE); } return 0;
    case WM_LBUTTONUP: if (g_drag >= 0) {
        ReleaseCapture();
        int c = Cell(), col, row;
        if (g_cfg.dk_snap) { col = (g_dragX - g_dragDX - g_work.left + c / 2) / c; row = (g_dragY - g_dragDY - g_work.top + c / 2) / c; }
        else               { col = (g_dragX - g_dragDX - g_work.left + c / 2) / c; row = (g_dragY - g_dragDY - g_work.top + c / 2) / c; }   /* positions are stored in cells */
        int cols = (g_work.right - g_work.left) / c, rows = (g_work.bottom - g_work.top) / c;
        if (col >= 0 && row >= 0 && col < cols && row < rows) {
            BOOL taken = FALSE; for (int k = 0; k < g_n; k++) if (k != g_drag && g_ic[k].col == col && g_ic[k].row == row) taken = TRUE;
            if (!taken) { g_ic[g_drag].col = col; g_ic[g_drag].row = row; SavePos(g_drag); }
        }
        g_drag = -1; InvalidateRect(h, NULL, FALSE);
    } return 0;
    case WM_LBUTTONDBLCLK: { int i = HitIcon((short)LOWORD(lp), (short)HIWORD(lp)); if (i >= 0) Shell_Run(g_ic[i].path); return 0; }
    case WM_RBUTTONUP: {
        int i = HitIcon((short)LOWORD(lp), (short)HIWORD(lp)); g_sel = i;
        HMENU mm = CreatePopupMenu();
        if (i >= 0) AppendMenuW(mm, MF_STRING, 1, L"Open");
        AppendMenuW(mm, MF_STRING, 2, L"Refresh");
        AppendMenuW(mm, MF_STRING, 3, L"Personalize / Settings");
        POINT pt; GetCursorPos(&pt);
        int c = TrackPopupMenu(mm, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, NULL);
        DestroyMenu(mm);
        if (c == 1) Shell_Run(g_ic[i].path); else if (c == 2) Rescan(); else if (c == 3) SettingsUI_Show(5);
        return 0; }
    case WM_TIMER: if (g_cfg.dk_widget_clock) { SYSTEMTIME st; GetLocalTime(&st); static WORD lm = 99; if (st.wMinute != lm) { lm = st.wMinute; InvalidateRect(h, NULL, FALSE); } } return 0;
    case WM_DISPLAYCHANGE: Desktop_Refresh(); return 0;
    case WM_APP_REFRESH: Desktop_Refresh(); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

void Desktop_Refresh(void)
{
    static int scannedIcons = -1;
    if (!g_dk) return;
    if (!g_cfg.dk_enabled) { ShowWindow(g_dk, SW_HIDE); return; }
    SetWindowPos(g_dk, HWND_BOTTOM, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (scannedIcons != g_cfg.dk_show_icons) { scannedIcons = g_cfg.dk_show_icons; Rescan(); }   /* folder scan only when needed */
    else { Place(); InvalidateRect(g_dk, NULL, FALSE); }
}

BOOL Desktop_Create(void)
{
    WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = DkProc; wc.hInstance = g_hInst; wc.lpszClassName = L"MontyDesktop"; wc.style = CS_DBLCLKS; wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    if (!RegisterClassW(&wc)) return FALSE;
    g_dk = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"Desktop", WS_POPUP, 0, 0, 10, 10, NULL, NULL, g_hInst, NULL);
    if (!g_dk) return FALSE;
    SetTimer(g_dk, 1, 30000, NULL);
    Desktop_Refresh();
    return TRUE;
}
