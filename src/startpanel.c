/* startpanel.c - Start panel: profile, app list, pinned tiles, power/vertical bar. */
#include "app.h"
#include <string.h>
#include <stdlib.h>

HWND g_hStart;
static Palette g_pal;
static int  g_hot = -1, g_scroll, g_listCount;
static int  g_filter_mode;                          /* 0 all, 1 favourites(pinned), 2 recent */
static wchar_t g_query[64];
static BOOL g_searchFocus;
static int  g_drag = -1, g_dragFrom = -1;            /* pinned tile being dragged */
static BOOL g_resizing; static int g_resizeIdx;
static int  g_tileOrder[LIST_MAX];
static int  g_tileSpan[LIST_MAX];                     /* per tile: 1 = small, 2 = wide / large, from drag-resize */
static int  g_animFrom[LIST_MAX], g_animTick;
static wchar_t g_recent[8][MAX_PATH]; static int g_recentN;

#define HIT_MAX 400
typedef struct { int kind; int idx; RECT r; } Hit;
enum { H_APP, H_TILE, H_POWER, H_SETTINGS, H_RUN, H_PROG, H_SYS, H_COLL_LIST, H_COLL_TILES, H_PLUS,
       H_FILTER_ALL, H_FILTER_FAV, H_FILTER_REC, H_SEARCH, H_USER, H_RESIZE };
static Hit g_hits[HIT_MAX]; static int g_nHits;

static void Hit_Add(int k, int i, RECT r) { if (g_nHits < HIT_MAX) { g_hits[g_nHits].kind = k; g_hits[g_nHits].idx = i; g_hits[g_nHits].r = r; g_nHits++; } }

static void SizeFor(int *w, int *h)
{
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    static const int W[] = { 360, 520, 720 }, Hh[] = { 480, 580, 680 };
    int s = g_cfg.st_size;
    if (s >= 3) { *w = sw; *h = sh - Taskbar_Thickness() - g_cfg.tb_detach; return; }
    *w = UiScale(W[s]); *h = UiScale(Hh[s]);
    if (*w > sw) *w = sw; if (*h > sh - Taskbar_Thickness() - 8) *h = sh - Taskbar_Thickness() - 8;
}

static void Reposition(void)
{
    int w, h, x, y, sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int t = Taskbar_Thickness() + g_cfg.tb_detach + 4;
    SizeFor(&w, &h);
    BOOL right = g_cfg.tb_align == 2 && g_cfg.tb_position < 2;       /* start button lives on the right corner */
    switch (g_cfg.tb_position) {
    case 1:  x = right ? sw - w : 0; y = t; break;
    case 2:  x = t; y = sh - h; break;
    case 3:  x = sw - w - t; y = sh - h; break;
    default: x = right ? sw - w : 0; y = sh - h - t; break;
    }
    if (g_pal.centered && g_cfg.tb_position < 2 && !right) x = (sw - w) / 2;   /* Win11 / dock style: centred */
    if (g_cfg.st_size >= 3) { x = 0; y = (g_cfg.tb_position == 1) ? t : 0; }
    SetWindowPos(g_hStart, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
    Win_SetRound(g_hStart, g_pal.radius);
    Win_SetAlpha(g_hStart, g_cfg.st_alpha);
}

static void AddRecent(const wchar_t *p)
{
    for (int i = 0; i < g_recentN; i++) if (!lstrcmpiW(g_recent[i], p)) { for (int k = i; k > 0; k--) lstrcpyW(g_recent[k], g_recent[k - 1]); lstrcpyW(g_recent[0], p); return; }
    if (g_recentN < 8) g_recentN++;
    for (int k = g_recentN - 1; k > 0; k--) lstrcpyW(g_recent[k], g_recent[k - 1]);
    lstrcpynW(g_recent[0], p, MAX_PATH);
}

static void Launch(const wchar_t *p) { AddRecent(p); StartPanel_Hide(); Shell_Run(p); }

/* ------------------------------------------------------------------ list helpers */
static BOOL Matches(const wchar_t *path)
{
    if (!g_query[0]) return TRUE;
    wchar_t nm[128]; Shell_DisplayName(path, nm, 128);
    wchar_t a[128], b[64]; lstrcpynW(a, nm, 128); lstrcpynW(b, g_query, 64);
    CharLowerW(a); CharLowerW(b);
    return wcsstr(a, b) != NULL;
}

static int CmpPath(const void *a, const void *b)
{
    wchar_t x[128], y[128]; Shell_DisplayName((const wchar_t *)a, x, 128); Shell_DisplayName((const wchar_t *)b, y, 128);
    return lstrcmpiW(x, y);
}

/* ------------------------------------------------------------------ painting */
static void DrawSearch(HDC dc, RECT r)
{
    Draw_Round(dc, &r, Col_Blend(g_pal.panel, g_pal.panelFg, 10), g_searchFocus ? g_pal.accent : g_pal.border, g_pal.radius / 2);
    RECT t = r; t.left += 10;
    HFONT f = Theme_Font(g_cfg.p_fontsize, FALSE);
    if (g_query[0]) Draw_Text(dc, &t, g_query, g_pal.panelFg, f, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    else            Draw_Text(dc, &t, L"Search apps", g_pal.fgDim, f, DT_SINGLELINE | DT_VCENTER);
}

static void DrawChip(HDC dc, RECT r, const wchar_t *s, BOOL on, int kind)
{
    BOOL hot = g_nHits > 0 && g_hits[g_nHits - 1].kind == kind && g_hot == g_nHits - 1;
    Draw_Round(dc, &r, on ? g_pal.accent : (hot ? g_pal.hover : Col_Blend(g_pal.panel, g_pal.panelFg, 8)), CLR_NONE, g_pal.radius / 2);
    Draw_Text(dc, &r, s, on ? Col_Text(g_pal.accent) : g_pal.panelFg, Theme_Font(g_cfg.p_fontsize > 1 ? g_cfg.p_fontsize - 1 : 1, FALSE), DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

static void Paint(HDC wdc)
{
    Theme_Resolve(&g_pal);
    RECT rc; GetClientRect(g_hStart, &rc);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bm = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(dc, bm);
    g_nHits = 0;

    BOOL horiz = (g_cfg.st_vbar_pos <= 1);                 /* vertical bar on left/right; else a horizontal strip */
    int barW = UiScale(g_pal.classic ? 34 : 48);
    HFONT f = Theme_Font(g_cfg.p_fontsize, FALSE);
    int pad = UiScale(10);

    /* ---- panel background (theme colour / picture / transparent) ---- */
    COLORREF panelCol = g_pal.panel;
    if (g_cfg.st_panel_bg == 2) panelCol = g_pal.bg;
    Draw_Surface(dc, &rc, &g_pal, panelCol, g_cfg.p_fx_start);
    if (g_cfg.st_panel_bg == 1) { HBITMAP b = Img_Load(g_cfg.start_panel_pic); if (b) { Img_Draw(dc, &rc, b, 0); Draw_Alpha(dc, &rc, g_pal.panel, 120); } }
    if (g_pal.classic && g_pal.bevel) { Draw_Bevel(dc, &rc, TRUE); }
    else { HBRUSH fb = CreateSolidBrush(g_pal.border); FrameRect(dc, &rc, fb); DeleteObject(fb); }

    /* ---- vertical (power) bar ---- */
    RECT bar;
    switch (g_cfg.st_vbar_pos) {
    case 1:  bar = (RECT){ rc.right - barW, 0, rc.right, rc.bottom }; break;
    case 2:  bar = (RECT){ 0, 0, rc.right, barW };  break;
    case 3:  bar = (RECT){ 0, rc.bottom - barW, rc.right, rc.bottom }; break;
    default: bar = (RECT){ 0, 0, barW, rc.bottom }; break;
    }
    if (g_cfg.st_vbar_bg == 1) { HBITMAP b = Img_Load(g_cfg.start_vbar_pic); if (b) Img_Draw(dc, &bar, b, 0); else Draw_Fill(dc, &bar, g_pal.bg); }
    else if (g_cfg.st_vbar_bg == 0) Draw_Fill(dc, &bar, g_pal.classic && g_pal.bevel ? g_pal.accent : Col_Blend(g_pal.bg, g_pal.fg, 3));
    else Draw_Alpha(dc, &bar, g_pal.bg, 90);
    if (g_pal.classic && g_pal.bevel && g_cfg.st_vbar_bg == 0) {          /* Win98: blue side strip with the vertical title */
        RECT t = bar; t.bottom -= 6;
        HFONT vf = CreateFontW(UiScale(20), 0, 900, 900, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Arial");
        HGDIOBJ of = SelectObject(dc, vf); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255,255,255));
        TextOutW(dc, t.left + 6, t.bottom - 4, L"Windows 98", 10);
        SelectObject(dc, of); DeleteObject(vf);
    }

    /* the bar's own buttons (power / settings / run / programs / system / collapse) */
    COLORREF bf = (g_cfg.st_vbar_bg == 0 && g_pal.classic && g_pal.bevel) ? RGB(255,255,255) : Col_Text(Col_Blend(g_pal.bg, g_pal.fg, 3));
    {
        static const struct { int kind, glyph; } btn[] = {
            { H_POWER, G_POWER }, { H_SETTINGS, G_GEAR }, { H_RUN, G_RUN }, { H_PROG, G_DESKTOP }, { H_SYS, G_TASKMGR },
            { H_COLL_LIST, G_CHEV_UP }, { H_COLL_TILES, G_GRID } };
        int n = sizeof btn / sizeof btn[0], bs = barW;
        BOOL bottomUp = !horiz ? FALSE : TRUE;
        int x = bar.left, y = bar.bottom;
        if (!(g_pal.classic && g_pal.bevel && g_cfg.st_vbar_bg == 0)) {
            for (int i = 0; i < n; i++) {
                RECT b;
                if (horiz) { b = (RECT){ bar.left, bar.bottom - (i + 1) * bs, bar.right, bar.bottom - i * bs }; }
                else       { b = (RECT){ bar.left + i * bs, bar.top, bar.left + (i + 1) * bs, bar.bottom }; }
                (void)bottomUp; (void)x; (void)y;
                if (b.top < bar.top + bs && horiz) break;
                BOOL hot = FALSE; Hit_Add(btn[i].kind, 0, b);
                hot = (g_hot == g_nHits - 1);
                if (hot) Draw_Round(dc, &b, Col_Blend(g_pal.bg, g_pal.fg, 18), CLR_NONE, g_pal.radius / 2);
                int gl = btn[i].glyph;
                if (btn[i].kind == H_COLL_LIST) gl = g_cfg.st_collapse_list ? G_CHEV_DOWN : G_CHEV_UP;
                Draw_Glyph(dc, &b, gl, bf, g_cfg.p_iconpreset);
            }
        } else {                                                /* classic: only the essentials, text-less strip */
            RECT b = { bar.left, bar.bottom - bs, bar.right, bar.bottom }; Hit_Add(H_POWER, 0, b);
            Draw_Glyph(dc, &b, G_POWER, bf, g_cfg.p_iconpreset);
            b = (RECT){ bar.left, bar.bottom - 2 * bs, bar.right, bar.bottom - bs }; Hit_Add(H_SETTINGS, 0, b);
            Draw_Glyph(dc, &b, G_GEAR, bf, g_cfg.p_iconpreset);
            b = (RECT){ bar.left, bar.bottom - 3 * bs, bar.right, bar.bottom - 2 * bs }; Hit_Add(H_RUN, 0, b);
            Draw_Glyph(dc, &b, G_RUN, bf, g_cfg.p_iconpreset);
        }
    }

    /* ---- content area (rc minus bar) ---- */
    RECT ct = rc;
    switch (g_cfg.st_vbar_pos) { case 1: ct.right -= barW; break; case 2: ct.top += barW; break; case 3: ct.bottom -= barW; break; default: ct.left += barW; }
    InflateRect(&ct, -pad, -pad);

    /* profile row: picture + name */
    int pic = UiScale(40);
    RECT pr = { ct.left, ct.top, ct.left + pic, ct.top + pic };
    Hit_Add(H_USER, 0, pr);
    Img_DrawCircle(dc, &pr, Img_Load(g_cfg.user_pic), g_pal.accent);
    if (!g_cfg.user_pic[0] || !Img_Load(g_cfg.user_pic)) {
        RECT in = { pr.left + pic / 4, pr.top + pic / 5, pr.right - pic / 4, pr.top + pic * 11 / 20 };
        Draw_Round(dc, &in, Col_Text(g_pal.accent), CLR_NONE, pic / 5);
    }
    RECT nm = { pr.right + pad, pr.top, ct.right, pr.bottom };
    Draw_Text(dc, &nm, g_cfg.user_name, g_pal.panelFg, Theme_Font(g_cfg.p_fontsize + 2, TRUE), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    /* search + "+" add button + filter chips */
    int sy = pr.bottom + pad, sh = UiScale(28);
    RECT plus = { ct.right - sh, sy, ct.right, sy + sh };
    RECT srch = { ct.left, sy, plus.left - 6, sy + sh };
    Hit_Add(H_SEARCH, 0, srch); DrawSearch(dc, srch);
    Hit_Add(H_PLUS, 0, plus);
    Draw_Round(dc, &plus, g_hot == g_nHits - 1 ? g_pal.hover : Col_Blend(g_pal.panel, g_pal.panelFg, 8), CLR_NONE, g_pal.radius / 2);
    Draw_Glyph(dc, &plus, G_PLUS, g_pal.panelFg, g_cfg.p_iconpreset);

    int cy0 = sy + sh + 6, ch = UiScale(22), cw = (ct.right - ct.left - 12) / 3;
    RECT c1 = { ct.left, cy0, ct.left + cw, cy0 + ch }, c2 = { c1.right + 6, cy0, c1.right + 6 + cw, cy0 + ch }, c3 = { c2.right + 6, cy0, ct.right, cy0 + ch };
    Hit_Add(H_FILTER_ALL, 0, c1); DrawChip(dc, c1, L"All", g_filter_mode == 0, H_FILTER_ALL);
    Hit_Add(H_FILTER_FAV, 0, c2); DrawChip(dc, c2, L"Favourites", g_filter_mode == 1, H_FILTER_FAV);
    Hit_Add(H_FILTER_REC, 0, c3); DrawChip(dc, c3, L"Recent", g_filter_mode == 2, H_FILTER_REC);

    /* split: app list on the left, tiles on the right (stacked when the panel is narrow) */
    int top = cy0 + ch + pad;
    int totalW = ct.right - ct.left;
    BOOL showList = !g_cfg.st_collapse_list, showTiles = !g_cfg.st_collapse_tiles;
    BOOL wide = totalW >= UiScale(440);
    RECT lr = { ct.left, top, ct.right, ct.bottom }, tr = { ct.left, top, ct.right, ct.bottom };
    if (showList && showTiles) {
        if (wide) { lr.right = ct.left + totalW * 45 / 100; tr.left = lr.right + pad; }
        else      { int mid = top + (ct.bottom - top) / 2; lr.bottom = mid - 4; tr.top = mid + 4; }
    }

    /* ---- app list ---- */
    int rowH = UiScale(30);
    if (showList) {
        int list = LIST_START, n = AppList_Count(list);
        if (g_filter_mode == 1) list = LIST_PINNED, n = AppList_Count(list);
        wchar_t paths[LIST_MAX][MAX_PATH]; int cnt = 0;
        if (g_filter_mode == 2) { for (int i = 0; i < g_recentN; i++) if (Matches(g_recent[i])) lstrcpyW(paths[cnt++], g_recent[i]); }
        else for (int i = 0; i < n && cnt < LIST_MAX; i++) { const wchar_t *p = AppList_Get(list, i); if (Matches(p)) lstrcpyW(paths[cnt++], p); }
        if (g_filter_mode == 0) qsort(paths, cnt, sizeof paths[0], CmpPath);
        g_listCount = cnt;
        int vis = (lr.bottom - lr.top) / rowH, maxScroll = cnt > vis ? cnt - vis : 0;
        if (g_scroll > maxScroll) g_scroll = maxScroll; if (g_scroll < 0) g_scroll = 0;
        int save = SaveDC(dc); IntersectClipRect(dc, lr.left, lr.top, lr.right, lr.bottom);
        for (int i = 0; i < vis && i + g_scroll < cnt; i++) {
            RECT r = { lr.left, lr.top + i * rowH, lr.right, lr.top + (i + 1) * rowH };
            Hit_Add(H_APP, i + g_scroll, r);
            if (g_hot == g_nHits - 1) Draw_Round(dc, &r, g_pal.hover, CLR_NONE, g_pal.radius / 2);
            int is = UiScale(20);
            Draw_Icon(dc, r.left + 6, r.top + (rowH - is) / 2, is, Icon_ForPath(paths[i + g_scroll], FALSE), &g_pal);
            wchar_t name[96]; Shell_DisplayName(paths[i + g_scroll], name, 96);
            RECT t = { r.left + is + 14, r.top, r.right - 4, r.bottom };
            Draw_Text(dc, &t, name, g_pal.panelFg, f, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        RestoreDC(dc, save);
        if (!cnt) Draw_Text(dc, &lr, g_filter_mode == 2 ? L"Nothing opened yet" : L"No apps found.\nUse + to add one.", g_pal.fgDim, f, DT_CENTER | DT_VCENTER);
    }

    /* ---- pinned tiles ---- */
    if (showTiles) {
        int n = AppList_Count(LIST_PINNED), ts = UiScale(g_cfg.st_tile_size), gap = 6;
        int cols = (tr.right - tr.left + gap) / (ts + gap); if (cols < 1) cols = 1;
        int x0 = tr.left, y0 = tr.top;
        int save = SaveDC(dc); IntersectClipRect(dc, tr.left, tr.top, tr.right, tr.bottom);
        /* flow layout with per-tile span (1 or 2 cells wide) */
        int cx = 0, cyy = 0;
        for (int k = 0; k < n; k++) {
            int i = g_tileOrder[k] < n ? g_tileOrder[k] : k;
            int span = g_tileSpan[i] > 1 && cols > 1 ? 2 : 1;
            if (cx + span > cols) { cx = 0; cyy++; }
            RECT r = { x0 + cx * (ts + gap), y0 + cyy * (ts + gap), x0 + cx * (ts + gap) + span * ts + (span - 1) * gap, y0 + cyy * (ts + gap) + ts };
            cx += span;
            if (r.top > tr.bottom) break;
            Hit_Add(H_TILE, i, r);
            BOOL hot = (g_hot == g_nHits - 1), dragging = (g_drag == i);
            COLORREF base = g_cfg.st_tile_colors ? Col_Tile(i) : g_pal.tile;
            COLORREF col = hot ? Col_Blend(base, RGB(255,255,255), 14) : base;
            if (dragging) { OffsetRect(&r, 3, 3); }
            Draw_Alpha(dc, &r, col, g_cfg.st_tile_alpha);
            if (g_pal.classic && g_pal.bevel) Draw_Bevel(dc, &r, TRUE);
            const wchar_t *p = AppList_Get(LIST_PINNED, i);
            int is = ts >= 70 ? 32 : 24;
            Draw_Icon(dc, (r.left + r.right) / 2 - is / 2, r.top + (ts - is) / 2 - 6, is, Icon_ForPath(p, TRUE), &g_pal);
            wchar_t name[48]; Shell_DisplayName(p, name, 48);
            RECT t = { r.left + 3, r.bottom - 18, r.right - 3, r.bottom - 2 };
            Draw_Text(dc, &t, name, Col_Text(base), Theme_Font(g_cfg.p_fontsize > 1 ? g_cfg.p_fontsize - 1 : 1, FALSE), DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS);
            /* resize grip: bottom-right corner, drag to widen */
            RECT gr = { r.right - 10, r.bottom - 10, r.right, r.bottom };
            Hit_Add(H_RESIZE, i, gr);
            if (hot || g_resizing) { HPEN pn = CreatePen(PS_SOLID, 1, Col_Text(base)); HGDIOBJ o = SelectObject(dc, pn);
                MoveToEx(dc, r.right - 3, r.bottom - 8, NULL); LineTo(dc, r.right - 8, r.bottom - 3);
                MoveToEx(dc, r.right - 3, r.bottom - 5, NULL); LineTo(dc, r.right - 5, r.bottom - 3); SelectObject(dc, o); DeleteObject(pn); }
        }
        if (!n) Draw_Text(dc, &tr, L"Pinned tiles appear here.\nUse + to pin apps.", g_pal.fgDim, f, DT_CENTER | DT_VCENTER);
        RestoreDC(dc, save);
    }
    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob); DeleteObject(bm); DeleteDC(dc);
}

/* ------------------------------------------------------------------ input */
static int HitAt(int x, int y)
{
    POINT p = { x, y };
    for (int i = g_nHits - 1; i >= 0; i--) if (PtInRect(&g_hits[i].r, p)) return i;   /* later = drawn on top */
    return -1;
}

static BOOL PickFile(wchar_t *out, HWND owner)
{
    OPENFILENAMEW o; ZeroMemory(&o, sizeof o); out[0] = 0;
    o.lStructSize = sizeof o; o.hwndOwner = owner;
    o.lpstrFilter = L"Programs (*.exe;*.lnk;*.bat)\0*.exe;*.lnk;*.bat\0All files\0*.*\0";
    o.lpstrFile = out; o.nMaxFile = MAX_PATH; o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&o);
}

static void AddMenu(HWND h)
{
    enum { A_START = 1, A_PIN, A_TB, A_TRAY, A_SCAN, A_SYS, A_CPL };
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, A_START, L"Add program to Start list...");
    AppendMenuW(m, MF_STRING, A_PIN,   L"Add pinned tile...");
    AppendMenuW(m, MF_STRING, A_TB,    L"Add taskbar shortcut...");
    AppendMenuW(m, MF_STRING, A_TRAY,  L"Add tray app...");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, A_SCAN,  L"Scan installed apps");
    AppendMenuW(m, MF_STRING, A_SYS,   L"Add system apps");
    AppendMenuW(m, MF_STRING, A_CPL,   L"Add Control Panel apps");
    POINT pt; GetCursorPos(&pt);
    int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, NULL);
    DestroyMenu(m);
    wchar_t f[MAX_PATH], sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
    switch (c) {
    case A_START: if (PickFile(f, h)) AppList_Add(LIST_START, f);  break;
    case A_PIN:   if (PickFile(f, h)) AppList_Add(LIST_PINNED, f); break;
    case A_TB:    if (PickFile(f, h)) AppList_Add(LIST_TASKBAR, f); break;
    case A_TRAY:  if (PickFile(f, h)) AppList_Add(LIST_TRAY, f);   break;
    case A_SCAN:  Shell_ScanStartMenu(LIST_START); break;
    case A_SYS: { static const wchar_t *s[] = { L"notepad.exe", L"regedit.exe", L"taskmgr.exe", L"winecfg.exe", L"cmd.exe", L"explorer.exe", L"wordpad.exe", L"winefile.exe" };
        for (int i = 0; i < 8; i++) { wsprintfW(f, L"%s\\%s", sys, s[i]); if (GetFileAttributesW(f) != INVALID_FILE_ATTRIBUTES) AppList_Add(LIST_START, f); }
        break; }
    case A_CPL: { static const wchar_t *s[] = { L"control.exe", L"appwiz.cpl", L"main.cpl", L"desk.cpl", L"inetcpl.cpl", L"joy.cpl" };
        for (int i = 0; i < 6; i++) { wsprintfW(f, L"%s\\%s", sys, s[i]); if (GetFileAttributesW(f) != INVALID_FILE_ATTRIBUTES) AppList_Add(LIST_START, f); }
        break; }
    }
    if (c) { Settings_Save(); PostMessageW(g_hTaskbar, WM_APP_REFRESH, 0, 0); }
}

static void AppMenu(HWND h, int list, int idx)
{
    enum { M_OPEN = 1, M_START, M_PIN, M_TB, M_TRAY, M_REMOVE };
    const wchar_t *path = list == LIST_PINNED ? AppList_Get(LIST_PINNED, idx) : AppList_Get(LIST_START, idx);
    wchar_t keep[MAX_PATH]; lstrcpyW(keep, path);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, M_OPEN, L"Open");
    AppendMenuW(m, MF_STRING, M_PIN, L"Pin to Start tiles");
    AppendMenuW(m, MF_STRING, M_TB, L"Pin to taskbar");
    AppendMenuW(m, MF_STRING, M_TRAY, L"Add to taskbar tray");
    AppendMenuW(m, MF_STRING, M_REMOVE, list == LIST_PINNED ? L"Unpin tile" : L"Remove from list");
    POINT pt; GetCursorPos(&pt);
    int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, NULL);
    DestroyMenu(m);
    switch (c) {
    case M_OPEN: Launch(keep); return;
    case M_PIN: AppList_Add(LIST_PINNED, keep); break;
    case M_TB:  AppList_Add(LIST_TASKBAR, keep); break;
    case M_TRAY:AppList_Add(LIST_TRAY, keep); break;
    case M_REMOVE: AppList_Remove(list, idx); break;
    default: return;
    }
    Settings_Save(); PostMessageW(g_hTaskbar, WM_APP_REFRESH, 0, 0);
}

static void PowerMenu(HWND h)
{
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 1, L"Restart shell (reload look)");
    AppendMenuW(m, MF_STRING, 2, L"Exit shell");
    AppendMenuW(m, MF_STRING, 3, L"Shut down container (stop Wine)");
    POINT pt; GetCursorPos(&pt);
    int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, NULL);
    DestroyMenu(m);
    StartPanel_Hide();
    if (c == 1) Shell_PowerAction(2); else if (c == 2) Shell_PowerAction(1); else if (c == 3) Shell_PowerAction(3);
}

static void SysMenu(HWND h)
{
    static const struct { const wchar_t *n, *e; } s[] = {
        { L"Control Panel", L"control.exe" }, { L"Registry Editor", L"regedit.exe" }, { L"Wine Configuration", L"winecfg.exe" },
        { L"Task Manager", L"taskmgr.exe" }, { L"Command Prompt", L"cmd.exe" }, { L"File Explorer", L"explorer.exe" },
        { L"Uninstall programs", L"uninstaller.exe" } };
    HMENU m = CreatePopupMenu();
    for (int i = 0; i < 7; i++) AppendMenuW(m, MF_STRING, i + 1, s[i].n);
    POINT pt; GetCursorPos(&pt);
    int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, NULL);
    DestroyMenu(m);
    if (c) { StartPanel_Hide(); Shell_Run(s[c - 1].e); }
}

static void RunDialog(HWND h)
{
    typedef void (WINAPI *RunDlg)(HWND, HICON, LPCWSTR, LPCWSTR, LPCWSTR, UINT);
    RunDlg fn = (RunDlg)GetProcAddress(GetModuleHandleW(L"shell32.dll"), MAKEINTRESOURCEA(61));
    StartPanel_Hide();
    if (fn) fn(NULL, NULL, NULL, L"Run", L"Type the name of a program, folder or document.", 0);
    else { wchar_t f[MAX_PATH]; if (PickFile(f, NULL)) Shell_Run(f); }
    (void)h;
}

static void MoveTile(int from, int to)
{
    int span = g_tileSpan[from];
    AppList_Move(LIST_PINNED, from, to);
    if (from < to) for (int k = from; k < to; k++) g_tileSpan[k] = g_tileSpan[k + 1];
    else           for (int k = from; k > to; k--) g_tileSpan[k] = g_tileSpan[k - 1];
    g_tileSpan[to] = span;
    g_drag = to;
}

static void Click(HWND h, int n, BOOL right)
{
    const Hit *t = &g_hits[n];
    if (right) {
        if (t->kind == H_APP) {
            wchar_t sel[MAX_PATH]; (void)sel;
            int list = g_filter_mode == 1 ? LIST_PINNED : LIST_START;
            if (g_filter_mode != 2) {
                /* map the visible row back to the underlying list index by path */
                int cnt = 0; wchar_t paths[LIST_MAX][MAX_PATH];
                for (int i = 0; i < AppList_Count(list) && cnt < LIST_MAX; i++) if (Matches(AppList_Get(list, i))) lstrcpyW(paths[cnt++], AppList_Get(list, i));
                if (g_filter_mode == 0) qsort(paths, cnt, sizeof paths[0], CmpPath);
                if (t->idx < cnt) for (int i = 0; i < AppList_Count(list); i++) if (!lstrcmpiW(AppList_Get(list, i), paths[t->idx])) { AppMenu(h, list, i); break; }
            }
        } else if (t->kind == H_TILE) AppMenu(h, LIST_PINNED, t->idx);
        return;
    }
    switch (t->kind) {
    case H_APP: {
        wchar_t paths[LIST_MAX][MAX_PATH]; int cnt = 0, list = g_filter_mode == 1 ? LIST_PINNED : LIST_START;
        if (g_filter_mode == 2) { for (int i = 0; i < g_recentN; i++) if (Matches(g_recent[i])) lstrcpyW(paths[cnt++], g_recent[i]); }
        else { for (int i = 0; i < AppList_Count(list); i++) if (Matches(AppList_Get(list, i))) lstrcpyW(paths[cnt++], AppList_Get(list, i));
               if (g_filter_mode == 0) qsort(paths, cnt, sizeof paths[0], CmpPath); }
        if (t->idx < cnt) Launch(paths[t->idx]);
        break; }
    case H_TILE:    Launch(AppList_Get(LIST_PINNED, t->idx)); break;
    case H_POWER:   PowerMenu(h); break;
    case H_SETTINGS:StartPanel_Hide(); SettingsUI_Show(0); break;
    case H_RUN:     RunDialog(h); break;
    case H_PROG:    g_filter_mode = 0; g_query[0] = 0; g_scroll = 0; break;
    case H_SYS:     SysMenu(h); break;
    case H_COLL_LIST:  g_cfg.st_collapse_list  = !g_cfg.st_collapse_list;  Settings_Save(); break;
    case H_COLL_TILES: g_cfg.st_collapse_tiles = !g_cfg.st_collapse_tiles; Settings_Save(); break;
    case H_PLUS:    AddMenu(h); break;
    case H_FILTER_ALL: g_filter_mode = 0; g_scroll = 0; break;
    case H_FILTER_FAV: g_filter_mode = 1; g_scroll = 0; break;
    case H_FILTER_REC: g_filter_mode = 2; g_scroll = 0; break;
    case H_SEARCH:  g_searchFocus = TRUE; break;
    case H_USER:    StartPanel_Hide(); SettingsUI_Show(2); break;
    }
    InvalidateRect(h, NULL, FALSE);
}

static LRESULT CALLBACK StProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); Paint(dc); EndPaint(h, &ps); return 0; }
    case WM_ACTIVATE: if (LOWORD(wp) == WA_INACTIVE) { if (!g_resizing) ShowWindow(h, SW_HIDE); } return 0;   /* click elsewhere closes */
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        int n = HitAt(x, y);
        if (g_resizing && n >= 0) { /* handled below by dragging size */ }
        if (g_drag >= 0 && (wp & MK_LBUTTON)) {                       /* drag a tile onto another to reorder */
            for (int i = g_nHits - 1; i >= 0; i--)
                if (g_hits[i].kind == H_TILE && g_hits[i].idx != g_drag && PtInRect(&g_hits[i].r, (POINT){ x, y })) { MoveTile(g_drag, g_hits[i].idx); break; }
            InvalidateRect(h, NULL, FALSE);
        } else if (g_resizing && (wp & MK_LBUTTON)) {
            for (int i = 0; i < g_nHits; i++) if (g_hits[i].kind == H_TILE && g_hits[i].idx == g_resizeIdx) {
                int wantWide = x > g_hits[i].r.left + (g_hits[i].r.right - g_hits[i].r.left) * 3 / 2;
                int want = wantWide ? 2 : 1;
                if (want != g_tileSpan[g_resizeIdx]) { g_tileSpan[g_resizeIdx] = want; InvalidateRect(h, NULL, FALSE); }
            }
        }
        if (n != g_hot) { g_hot = n; InvalidateRect(h, NULL, FALSE); }
        if (n >= 0 && g_hits[n].kind == H_RESIZE) SetCursor(LoadCursorW(NULL, IDC_SIZENWSE)); else SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return 0; }
    case WM_LBUTTONDOWN: {
        int n = HitAt((short)LOWORD(lp), (short)HIWORD(lp));
        g_searchFocus = (n >= 0 && g_hits[n].kind == H_SEARCH);
        if (n >= 0 && g_hits[n].kind == H_RESIZE) { g_resizing = TRUE; g_resizeIdx = g_hits[n].idx; SetCapture(h); }
        else if (n >= 0 && g_hits[n].kind == H_TILE) { g_drag = g_hits[n].idx; g_dragFrom = g_drag; SetCapture(h); }
        InvalidateRect(h, NULL, FALSE);
        return 0; }
    case WM_LBUTTONUP: {
        int n = HitAt((short)LOWORD(lp), (short)HIWORD(lp));
        BOOL moved = g_drag >= 0 && g_drag != g_dragFrom, wasResize = g_resizing;
        if (g_drag >= 0 || g_resizing) ReleaseCapture();
        int d = g_drag; g_drag = -1; g_resizing = FALSE;
        if (moved || wasResize) { Settings_Save(); InvalidateRect(h, NULL, FALSE); return 0; }
        (void)d;
        if (n >= 0) Click(h, n, FALSE);
        return 0; }
    case WM_RBUTTONUP: { int n = HitAt((short)LOWORD(lp), (short)HIWORD(lp)); if (n >= 0) Click(h, n, TRUE); return 0; }
    case WM_MOUSEWHEEL: g_scroll -= (short)HIWORD(wp) / WHEEL_DELTA * 2; if (g_scroll < 0) g_scroll = 0; InvalidateRect(h, NULL, FALSE); return 0;
    case WM_CHAR: {                                            /* type anywhere to search */
        int l = lstrlenW(g_query);
        if (wp == 8) { if (l) g_query[l - 1] = 0; }
        else if (wp == 27) { if (l) g_query[0] = 0; else StartPanel_Hide(); }
        else if (wp == 13) { if (g_listCount) { RECT r = { 0, 0, 1, 1 }; (void)r; for (int i = 0; i < g_nHits; i++) if (g_hits[i].kind == H_APP) { Click(h, i, FALSE); break; } } }
        else if (wp >= 32 && l < 62) { g_query[l] = (wchar_t)wp; g_query[l + 1] = 0; }
        g_scroll = 0; g_filter_mode = (g_filter_mode == 2) ? 2 : (g_query[0] ? 0 : g_filter_mode);
        InvalidateRect(h, NULL, FALSE);
        return 0; }
    case WM_APP_REFRESH: StartPanel_Refresh(); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

void StartPanel_Hide(void) { if (g_hStart) ShowWindow(g_hStart, SW_HIDE); }

void StartPanel_Refresh(void)
{
    if (!g_hStart) return;
    Theme_Resolve(&g_pal);
    Reposition();
    InvalidateRect(g_hStart, NULL, FALSE);
}

void StartPanel_Toggle(void)
{
    if (IsWindowVisible(g_hStart)) { StartPanel_Hide(); return; }
    Theme_Resolve(&g_pal);
    int n = AppList_Count(LIST_PINNED);
    for (int i = 0; i < LIST_MAX; i++) g_tileOrder[i] = i;
    if (g_cfg.st_tile_random)                                  /* random arrangement each time the panel opens */
        for (int i = n - 1; i > 0; i--) { int j = rand() % (i + 1), t = g_tileOrder[i]; g_tileOrder[i] = g_tileOrder[j]; g_tileOrder[j] = t; }
    g_query[0] = 0; g_scroll = 0; g_hot = -1;
    if (!AppList_Count(LIST_START)) { Shell_ScanStartMenu(LIST_START); Settings_Save(); }
    Reposition();
    ShowWindow(g_hStart, SW_SHOW);
    SetForegroundWindow(g_hStart);
    InvalidateRect(g_hStart, NULL, FALSE);
}

BOOL StartPanel_Create(void)
{
    WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = StProc; wc.hInstance = g_hInst; wc.lpszClassName = L"MontyStart"; wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    if (!RegisterClassW(&wc)) return FALSE;
    g_hStart = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, wc.lpszClassName, L"Start", WS_POPUP, 0, 0, 400, 500, NULL, NULL, g_hInst, NULL);
    for (int i = 0; i < LIST_MAX; i++) { g_tileSpan[i] = 1; g_tileOrder[i] = i; }
    srand(GetTickCount());
    return g_hStart != NULL;
}
