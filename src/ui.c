/* ui.c - themed settings window. Table driven: every option is one row in a tab table.
 * Colours/fonts come from the active theme palette, so the window re-skins itself with the OS look. */
#include "app.h"
#include <stddef.h>
#include <commdlg.h>
#include <string.h>

enum { R_HEAD, R_TOGGLE, R_SLIDER, R_CHOICE, R_COLOR, R_PATH, R_TEXT, R_ACTION, R_NOTE };
enum { A_RV_ADD = 1, A_RV_CLEAR, A_TWEAKS, A_BACKUP, A_RESTORE, A_RESET, A_RESTART, A_ABOUT, A_SCAN };

typedef struct { int type; const wchar_t *label; size_t off; int mn, mx; const wchar_t *opts; int id; } Row;

#define OFS(f)        offsetof(Settings, f)
#define HD(l)         { R_HEAD,   l, 0, 0, 0, NULL, 0 }
#define NT(l)         { R_NOTE,   l, 0, 0, 0, NULL, 0 }
#define TG(l,f)       { R_TOGGLE, l, OFS(f), 0, 1, NULL, 0 }
#define SL(l,f,a,b)   { R_SLIDER, l, OFS(f), a, b, NULL, 0 }
#define CH(l,f,o)     { R_CHOICE, l, OFS(f), 0, 0, o, 0 }
#define CL(l,f)       { R_COLOR,  l, OFS(f), 0, 0, NULL, 0 }
#define PX(l,f)       { R_PATH,   l, OFS(f), 0, 0, NULL, 0 }
#define TX(l,f)       { R_TEXT,   l, OFS(f), 0, 0, NULL, 0 }
#define AC(l,i)       { R_ACTION, l, 0, 0, 0, NULL, i }

static wchar_t g_themeOpts[700], g_ttOpts[700], g_presetOpts[400], g_modeOpts[100], g_fxOpts[100];

static const Row T_TASKBAR[] = {
    HD(L"Layout"),
    CH(L"Position", tb_position, L"Bottom|Top|Left|Right"),
    CH(L"Taskbar is", tb_zorder, L"Always in front of windows|Behind windows"),
    CH(L"Alignment", tb_align, L"Left|Middle|Right (start button on right corner)"),
    SL(L"Height (0 = auto from theme)", tb_height, 0, 96),
    TG(L"Auto-fit width to running apps", tb_autowidth),
    SL(L"Icon size", tb_iconsize, 12, 48),
    TG(L"Show window title on buttons", tb_showtitle),
    TG(L"Rounded corners", tb_rounded),
    SL(L"Detach from screen edge", tb_detach, 0, 40),
    CL(L"Taskbar colour (right-click = auto)", tb_color),
    TG(L"Running app underline", tb_underline),
    SL(L"Underline height", tb_underline_h, 1, 8),
    HD(L"Visibility"),
    TG(L"Hide Winlator's built-in taskbar", tb_hide_native),
    TG(L"Underline colour from app icon", tb_underline_adapt),
    CH(L"Over full-screen app", tb_fs_mode, L"Keep visible|Visible over app|Floating icon|Hide bar"),
    CH(L"Applies to", tb_applies_to, L"Full screen|Maximized|Both"),
    TG(L"Start automatically with the container", tb_autostart),
    SL(L"Startup delay (seconds)", tb_start_delay, 0, 60),
    TG(L"Live preview on hover", tb_preview),
    SL(L"Preview width", tb_prev_w, 120, 480),
    SL(L"Preview height", tb_prev_h, 80, 320),
    HD(L"Elements"),
    TG(L"Show desktop button", tb_show_desktop),
    TG(L"Clock", tb_clock),
    TG(L"Show weekday in clock", tb_clock_day),
    TG(L"Divider between pinned and running", tb_divider),
    CH(L"Divider thickness", tb_divider_w, L"Thin|Medium|Bold"),
    TG(L"Battery", tb_tray_battery),
    TG(L"Wi-Fi", tb_tray_wifi),
    TG(L"Bluetooth", tb_tray_bt),
    TG(L"Task manager", tb_tray_taskmgr),
    TG(L"Tray apps in a grid popup", tb_tray_grid),
};
static const Row T_REVEAL[] = {
    HD(L"Reveal bar"),
    NT(L"A thin bar shown while the taskbar is hidden. Hover it to slide the taskbar over the running app."),
    TG(L"Enable reveal bar", rv_enabled),
    TG(L"Also in normal mode (taskbar auto-hides)", rv_in_normal),
    CL(L"Colour (right-click = accent)", rv_color),
    CH(L"Position", rv_pos, L"Bottom|Top"),
    CH(L"Alignment", rv_align, L"Left|Middle|Right"),
    TG(L"Corner (L shape, left/right only)", rv_corner),
    SL(L"Opacity", rv_opacity, 30, 255),
    SL(L"Height", rv_height, 1, 24),
    SL(L"Width", rv_width, 40, 2000),
    SL(L"Taskbar visible for (seconds)", rv_seconds, 1, 15),
    CH(L"Reveal animation", rv_anim, L"Blink|Pop-up|Slide from edge|Slide from left|Slide from right|Fade"),
    HD(L"Exceptions"),
    AC(L"Add an app that never uses reveal bar...", A_RV_ADD),
    AC(L"Clear exception list", A_RV_CLEAR),
};
static const Row T_START[] = {
    HD(L"Start panel"),
    CH(L"Panel size", st_size, L"Normal|Medium|Large|Full screen"),
    CL(L"Start button colour (right-click = theme)", st_btn_color),
    PX(L"Start button picture", start_icon_path),
    TX(L"User name", user_name),
    PX(L"User picture", user_pic),
    CH(L"Side bar position", st_vbar_pos, L"Left|Right|Top|Bottom"),
    CH(L"Side bar background", st_vbar_bg, L"Theme colour|Picture|Transparent"),
    PX(L"Side bar picture", start_vbar_pic),
    CH(L"Panel background", st_panel_bg, L"Theme colour|Picture|Transparent"),
    PX(L"Panel picture", start_panel_pic),
    SL(L"Panel transparency (low = clear)", st_alpha, 60, 255),
    HD(L"Pinned tiles"),
    TG(L"Different colour per tile", st_tile_colors),
    SL(L"Tile opacity", st_tile_alpha, 60, 255),
    SL(L"Tile size", st_tile_size, 48, 160),
    TG(L"Random arrangement each time it opens", st_tile_random),
    TG(L"Collapse app list", st_collapse_list),
    TG(L"Collapse pinned tiles", st_collapse_tiles),
    NT(L"Drag a tile onto another to reorder. Drag its bottom-right corner to make it wide."),
};
static const Row T_TITLE[] = {
    HD(L"Window title bars"),
    TG(L"Custom title bars", tt_enabled),
    CH(L"Title bar style", tt_theme, L""),                        /* options filled at run time */
    SL(L"Transparency", tt_alpha, 80, 255),
    TG(L"Accent border (title + frame accent)", tt_apply_all),
    TG(L"Hide Wine's native title bar", tt_hide_native),
    TG(L"Pin-on-top button", tt_btn_pin),
    TG(L"Roll-up (shade) button", tt_btn_shade),
    CL(L"Custom title colour (right-click = theme)", tt_color),
    NT(L"The custom bar sits directly above each window's native bar, focused or not."),
};
static const Row T_PERSONAL[] = {
    HD(L"Look"),
    CH(L"OS theme", p_theme, L""),                                /* filled at run time */
    TG(L"Classic original look (exact old OS style)", p_classic),
    CH(L"Colour mode", p_mode, L""),
    CH(L"Effect", p_effect, L""),
    SL(L"Blur amount", p_blur, 0, 40),
    CL(L"Accent colour (right-click = theme)", p_accent),
    SL(L"Glass shine", p_shine, 0, 100),
    SL(L"Glass gloss", p_gloss, 0, 100),
    TG(L"Rounded corners everywhere", p_round),
    HD(L"Apply effect to"),
    TG(L"Taskbar", p_fx_taskbar), TG(L"Start panel", p_fx_start), TG(L"Preview", p_fx_preview),
    TG(L"Window title bars", p_fx_title), TG(L"This settings window", p_fx_install),
    HD(L"Icons & text"),
    CH(L"System icon preset", p_iconpreset, L""),
    TG(L"Recolour app icons to theme", p_icon_recolor),
    TG(L"Fill recoloured icons with accent", p_icon_fill),
    TX(L"Font", font_name),
    SL(L"Font size", p_fontsize, 6, 16),
    SL(L"UI scale %", p_uiscale, 70, 200),
};
static const Row T_DESKTOP[] = {
    HD(L"Desktop mode"),
    TG(L"Enable desktop mode", dk_enabled),
    TG(L"Show desktop icons", dk_show_icons),
    SL(L"Icon grid size", dk_grid, 48, 160),
    CH(L"Icons start in corner", dk_corner, L"Top-left|Top-right|Bottom-left|Bottom-right"),
    TG(L"Lock icon positions", dk_lock),
    TG(L"Clock widget", dk_widget_clock),
    PX(L"Wallpaper picture", wallpaper),
    CL(L"Background colour (right-click = theme)", dk_wall_color),
    CH(L"Wallpaper fit", dk_wall_fit, L"Crop to fill|Fit|Stretch|Centre|Tile"),
    SL(L"Font size", dk_fontsize, 6, 16),
    SL(L"UI scale %", dk_uiscale, 70, 200),
};
static const Row T_SYSTEM[] = {
    HD(L"Battery & performance"),
    SL(L"Taskbar refresh interval (ms)", perf_poll_ms, 150, 2000),
    TG(L"Battery saver (refresh slower on battery)", perf_battery_lite),
    AC(L"Apply Wine stability/performance tweaks now", A_TWEAKS),
    HD(L"Settings"),
    AC(L"Back up settings...", A_BACKUP),
    AC(L"Restore settings...", A_RESTORE),
    AC(L"Reset everything to defaults", A_RESET),
    AC(L"Restart shell", A_RESTART),
    HD(L"About"),
    AC(L"About / star on GitHub", A_ABOUT),
};

typedef struct { const wchar_t *name; const Row *rows; int n; } Tab;
#define TABN(a) (int)(sizeof a / sizeof a[0])
static const Tab g_tabs[] = {
    { L"Taskbar",     T_TASKBAR,  TABN(T_TASKBAR)  },
    { L"Reveal bar",  T_REVEAL,   TABN(T_REVEAL)   },
    { L"Start panel", T_START,    TABN(T_START)    },
    { L"Title bars",  T_TITLE,    TABN(T_TITLE)    },
    { L"Personalize", T_PERSONAL, TABN(T_PERSONAL) },
    { L"Desktop",     T_DESKTOP,  TABN(T_DESKTOP)  },
    { L"Apps",        NULL,       0                },
    { L"System",      T_SYSTEM,   TABN(T_SYSTEM)   },
};
#define NTABS 8
#define TAB_APPS 6

/* ---------------- state ---------------- */
HWND g_hSettings;
static Palette g_pal;
static int g_tab, g_scroll, g_contentH, g_hot = -1, g_sliderHit = -1, g_appList;
static HWND g_edit; static const Row *g_editRow;

enum { K_TAB, K_ROW, K_APPSEL, K_APPDEL, K_APPADD, K_APPSCAN };
typedef struct { int kind, a; RECT r; } Hit;
static Hit g_hits[300]; static int g_nHits;
static void AddHit(int k, int a, RECT r) { if (g_nHits < 300) { g_hits[g_nHits].kind = k; g_hits[g_nHits].a = a; g_hits[g_nHits].r = r; g_nHits++; } }

static int  *IP(const Row *r) { return (int *)((BYTE *)&g_cfg + r->off); }
static wchar_t *SP(const Row *r) { return (wchar_t *)((BYTE *)&g_cfg + r->off); }

static void BuildOptionStrings(void)
{
    g_themeOpts[0] = g_ttOpts[0] = g_presetOpts[0] = 0;
    lstrcpyW(g_ttOpts, L"Follow OS theme");
    for (int i = 0; i < THEME_COUNT; i++) {
        if (i) lstrcatW(g_themeOpts, L"|");
        lstrcatW(g_themeOpts, g_themes[i].name);
        if (i) { lstrcatW(g_ttOpts, L"|"); lstrcatW(g_ttOpts, g_themes[i].name); }
    }
    for (int i = 0; i < ICON_PRESETS; i++) { if (i) lstrcatW(g_presetOpts, L"|"); lstrcatW(g_presetOpts, g_presetNames[i]); }
    g_modeOpts[0] = g_fxOpts[0] = 0;
    for (int i = 0; i < 7; i++) { if (i) lstrcatW(g_modeOpts, L"|"); lstrcatW(g_modeOpts, g_modeNames[i]); }
    for (int i = 0; i < 6; i++) { if (i) lstrcatW(g_fxOpts, L"|"); lstrcatW(g_fxOpts, g_effectNames[i]); }
}

static const wchar_t *OptsFor(const Row *r)
{
    if (r->opts && r->opts[0]) return r->opts;
    if (r->off == OFS(p_theme)) return g_themeOpts;
    if (r->off == OFS(tt_theme)) return g_ttOpts;
    if (r->off == OFS(p_iconpreset)) return g_presetOpts;
    if (r->off == OFS(p_mode)) return g_modeOpts;
    if (r->off == OFS(p_effect)) return g_fxOpts;
    return L"";
}

static void OptName(const wchar_t *opts, int idx, wchar_t *out, int n)
{
    int k = 0; const wchar_t *s = opts;
    out[0] = 0;
    for (;;) {
        const wchar_t *e = wcschr(s, L'|');
        int len = e ? (int)(e - s) : lstrlenW(s);
        if (k == idx) { lstrcpynW(out, s, len + 1 < n ? len + 1 : n); return; }
        if (!e) break;
        s = e + 1; k++;
    }
}

static void ApplyAll(BOOL save)
{
    if (save) Settings_Save();
    App_ApplyAll();
    InvalidateRect(g_hSettings, NULL, FALSE);
}

/* ---------------- painting ---------------- */
static void DrawToggle(HDC dc, RECT r, BOOL on)
{
    int w = 44, h = 22;
    RECT t = { r.right - w, (r.top + r.bottom) / 2 - h / 2, r.right, (r.top + r.bottom) / 2 + h / 2 };
    Draw_Round(dc, &t, on ? g_pal.accent : Col_Blend(g_pal.panel, g_pal.panelFg, 25), CLR_NONE, h / 2);
    int k = h - 6, kx = on ? t.right - k - 3 : t.left + 3;
    RECT kn = { kx, t.top + 3, kx + k, t.top + 3 + k };
    Draw_Round(dc, &kn, on ? Col_Text(g_pal.accent) : g_pal.panelFg, CLR_NONE, k / 2);
}

static void Paint(HDC wdc)
{
    Theme_Resolve(&g_pal);
    RECT rc; GetClientRect(g_hSettings, &rc);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bm = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(dc, bm);
    g_nHits = 0;
    HFONT f = Theme_Font(g_cfg.p_fontsize, FALSE), fb = Theme_Font(g_cfg.p_fontsize, TRUE);
    int s = g_cfg.p_uiscale;
    int sideW = 160 * s / 100, rh = 40 * s / 100, hh = 34 * s / 100;

    Draw_Surface(dc, &rc, &g_pal, g_pal.panel, g_cfg.p_fx_install);
    RECT side = { 0, 0, sideW, rc.bottom };
    Draw_Fill(dc, &side, Col_Blend(g_pal.bg, g_pal.fg, 4));
    RECT ttl = { 12, 10, sideW - 8, 10 + 40 * s / 100 };
    Draw_Text(dc, &ttl, L"Taskbar & Desktop", g_pal.fg, fb, DT_WORDBREAK);
    for (int i = 0; i < NTABS; i++) {
        RECT r = { 6, 64 * s / 100 + i * 36 * s / 100, sideW - 6, 64 * s / 100 + (i + 1) * 36 * s / 100 - 2 };
        AddHit(K_TAB, i, r);
        BOOL on = (i == g_tab), hot = (g_hot == g_nHits - 1);
        if (on) Draw_Round(dc, &r, g_pal.accent, CLR_NONE, g_pal.radius / 2);
        else if (hot) Draw_Round(dc, &r, g_pal.hover, CLR_NONE, g_pal.radius / 2);
        RECT t = r; t.left += 12;
        Draw_Text(dc, &t, g_tabs[i].name, on ? Col_Text(g_pal.accent) : g_pal.fg, f, DT_SINGLELINE | DT_VCENTER);
    }

    /* content (clipped, scrolled) */
    RECT ct = { sideW + 10, 8, rc.right - 14, rc.bottom - 8 };
    int save = SaveDC(dc); IntersectClipRect(dc, ct.left, ct.top, ct.right, ct.bottom);
    int y = ct.top - g_scroll;
    int cw = ct.right - ct.left;
    COLORREF fg = g_pal.panelFg, dim = g_pal.fgDim;

    if (g_tab == TAB_APPS) {
        static const wchar_t *names[LIST_COUNT] = { L"Start app list", L"Pinned tiles", L"Taskbar shortcuts", L"Tray apps" };
        RECT hr = { ct.left, y, ct.right, y + hh };
        Draw_Text(dc, &hr, L"Add / remove applications", g_pal.accent, fb, DT_SINGLELINE | DT_VCENTER); y += hh;
        RECT sel = { ct.left, y, ct.right, y + rh - 6 };
        AddHit(K_APPSEL, 0, sel);
        Draw_Round(dc, &sel, Col_Blend(g_pal.panel, fg, 10), g_pal.border, g_pal.radius / 2);
        RECT st = sel; st.left += 12;
        Draw_Text(dc, &st, names[g_appList], fg, f, DT_SINGLELINE | DT_VCENTER);
        RECT chv = sel; chv.left = sel.right - 30; Draw_Glyph(dc, &chv, G_CHEV_DOWN, fg, 0);
        y += rh;
        RECT add = { ct.left, y, ct.left + cw / 2 - 4, y + rh - 6 }, scn = { add.right + 8, y, ct.right, y + rh - 6 };
        AddHit(K_APPADD, 0, add); AddHit(K_APPSCAN, 0, scn);
        Draw_Round(dc, &add, g_pal.accent, CLR_NONE, g_pal.radius / 2); Draw_Text(dc, &add, L"+ Add program...", Col_Text(g_pal.accent), f, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        Draw_Round(dc, &scn, Col_Blend(g_pal.panel, fg, 12), CLR_NONE, g_pal.radius / 2); Draw_Text(dc, &scn, L"Scan installed apps", fg, f, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        y += rh;
        for (int i = 0; i < AppList_Count(g_appList); i++) {
            RECT r = { ct.left, y, ct.right, y + rh - 4 };
            if (r.bottom > ct.top && r.top < ct.bottom) {
                wchar_t nm[96]; Shell_DisplayName(AppList_Get(g_appList, i), nm, 96);
                Draw_Icon(dc, r.left + 6, r.top + (rh - 4 - 20) / 2, 20, Icon_ForPath(AppList_Get(g_appList, i), FALSE), &g_pal);
                RECT t = { r.left + 36, r.top, r.right - 40, r.bottom };
                Draw_Text(dc, &t, nm, fg, f, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                RECT x = { r.right - 34, r.top, r.right, r.bottom };
                AddHit(K_APPDEL, i, x);
                if (g_hot == g_nHits - 1) Draw_Round(dc, &x, RGB(196,43,28), CLR_NONE, g_pal.radius / 2);
                Draw_Glyph(dc, &x, G_CLOSE, g_hot == g_nHits - 1 ? RGB(255,255,255) : dim, 0);
            }
            y += rh;
        }
    } else {
        const Tab *tb = &g_tabs[g_tab];
        for (int i = 0; i < tb->n; i++) {
            const Row *r = &tb->rows[i];
            if (r->type == R_HEAD) {
                RECT h = { ct.left, y + 6, ct.right, y + hh };
                Draw_Text(dc, &h, r->label, g_pal.accent, fb, DT_SINGLELINE | DT_VCENTER);
                y += hh; continue;
            }
            if (r->type == R_NOTE) {
                RECT n = { ct.left, y, ct.right, y + rh };
                Draw_Text(dc, &n, r->label, dim, Theme_Font(g_cfg.p_fontsize > 1 ? g_cfg.p_fontsize - 1 : 1, FALSE), DT_WORDBREAK);
                y += rh; continue;
            }
            RECT row = { ct.left, y, ct.right, y + rh };
            if (row.bottom > ct.top && row.top < ct.bottom) {
                AddHit(K_ROW, i, row);
                BOOL hot = (g_hot == g_nHits - 1);
                if (hot && r->type != R_SLIDER) Draw_Round(dc, &row, Col_Blend(g_pal.panel, fg, 6), CLR_NONE, g_pal.radius / 2);
                RECT lab = { row.left + 8, row.top, row.left + cw * 52 / 100, row.bottom };
                RECT ctl = { row.left + cw * 54 / 100, row.top, row.right - 8, row.bottom };
                Draw_Text(dc, &lab, r->label, fg, f, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                switch (r->type) {
                case R_TOGGLE: DrawToggle(dc, ctl, *IP(r) != 0); break;
                case R_SLIDER: {
                    int v = *IP(r), span = r->mx - r->mn;
                    RECT tr = { ctl.left, (ctl.top + ctl.bottom) / 2 - 2, ctl.right - 48, (ctl.top + ctl.bottom) / 2 + 2 };
                    RECT hit = { ctl.left, ctl.top, ctl.right - 44, ctl.bottom };
                    g_hits[g_nHits - 1].r = hit;                       /* the slider owns only its track area */
                    Draw_Round(dc, &tr, Col_Blend(g_pal.panel, fg, 22), CLR_NONE, 2);
                    int kx = tr.left + (span ? (tr.right - tr.left) * (v - r->mn) / span : 0);
                    RECT fl = tr; fl.right = kx; Draw_Round(dc, &fl, g_pal.accent, CLR_NONE, 2);
                    RECT kn = { kx - 8, (ctl.top + ctl.bottom) / 2 - 8, kx + 8, (ctl.top + ctl.bottom) / 2 + 8 };
                    Draw_Round(dc, &kn, g_pal.accent, g_pal.panel, 8);
                    wchar_t b[16]; wsprintfW(b, L"%d", v);
                    RECT vt = { ctl.right - 44, ctl.top, ctl.right, ctl.bottom };
                    Draw_Text(dc, &vt, b, dim, f, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
                    break; }
                case R_CHOICE: {
                    RECT fld = { ctl.left, ctl.top + 5, ctl.right, ctl.bottom - 5 };
                    Draw_Round(dc, &fld, Col_Blend(g_pal.panel, fg, 10), g_pal.border, g_pal.radius / 2);
                    wchar_t nm[64]; OptName(OptsFor(r), *IP(r), nm, 64);
                    RECT t = fld; t.left += 10; t.right -= 26;
                    Draw_Text(dc, &t, nm, fg, f, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                    RECT c = fld; c.left = fld.right - 26; Draw_Glyph(dc, &c, G_CHEV_DOWN, fg, 0);
                    break; }
                case R_COLOR: {
                    int v = *IP(r);
                    RECT sw = { ctl.right - 52, ctl.top + 7, ctl.right, ctl.bottom - 7 };
                    COLORREF show = v == -1 ? g_pal.accent : (COLORREF)v;
                    Draw_Round(dc, &sw, show, g_pal.border, 4);
                    RECT t = { ctl.left, ctl.top, sw.left - 8, ctl.bottom };
                    Draw_Text(dc, &t, v == -1 ? L"Auto (follows theme)" : L"Custom", dim, f, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
                    break; }
                case R_PATH: {
                    RECT b = { ctl.left, ctl.top + 5, ctl.right, ctl.bottom - 5 };
                    Draw_Round(dc, &b, Col_Blend(g_pal.panel, fg, 10), g_pal.border, g_pal.radius / 2);
                    const wchar_t *p = SP(r); const wchar_t *nm = wcsrchr(p, L'\\'); nm = nm ? nm + 1 : p;
                    RECT t = b; t.left += 10; t.right -= 8;
                    Draw_Text(dc, &t, p[0] ? nm : L"Choose picture...  (right-click = clear)", p[0] ? fg : dim, f, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                    break; }
                case R_TEXT: {
                    RECT b = { ctl.left, ctl.top + 5, ctl.right, ctl.bottom - 5 };
                    Draw_Round(dc, &b, Col_Blend(g_pal.panel, fg, 10), g_pal.border, g_pal.radius / 2);
                    RECT t = b; t.left += 10;
                    Draw_Text(dc, &t, SP(r), fg, f, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                    break; }
                case R_ACTION: {
                    RECT b = { row.left + 8, row.top + 4, row.right - 8, row.bottom - 4 };
                    Draw_Round(dc, &b, hot ? g_pal.accent : Col_Blend(g_pal.panel, fg, 12), CLR_NONE, g_pal.radius / 2);
                    RECT t = b; t.left += 12;
                    Draw_Text(dc, &t, r->label, hot ? Col_Text(g_pal.accent) : fg, f, DT_SINGLELINE | DT_VCENTER);
                    break; }
                }
            }
            y += rh;
        }
    }
    g_contentH = y + g_scroll - ct.top;
    RestoreDC(dc, save);
    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob); DeleteObject(bm); DeleteDC(dc);
}

/* ---------------- input ---------------- */
static BOOL PickPath(HWND h, wchar_t *out, BOOL save, const wchar_t *filter, const wchar_t *defExt)
{
    OPENFILENAMEW o; ZeroMemory(&o, sizeof o);
    o.lStructSize = sizeof o; o.hwndOwner = h; o.lpstrFilter = filter; o.lpstrFile = out; o.nMaxFile = MAX_PATH;
    o.lpstrDefExt = defExt; o.Flags = OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST));
    return save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
}

static const wchar_t *k_imgFilter = L"Pictures\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0All files\0*.*\0";
static const wchar_t *k_exeFilter = L"Programs\0*.exe;*.lnk;*.bat\0All files\0*.*\0";

static void CommitEdit(void)
{
    if (!g_edit) return;
    wchar_t buf[260]; GetWindowTextW(g_edit, buf, 260);
    int cap = (g_editRow->off == OFS(user_name)) ? 64 : 32;
    lstrcpynW(SP(g_editRow), buf, cap);
    DestroyWindow(g_edit); g_edit = NULL; g_editRow = NULL;
    ApplyAll(TRUE);
}

static void DoAction(HWND h, int id)
{
    wchar_t f[MAX_PATH]; f[0] = 0;
    switch (id) {
    case A_RV_ADD: if (PickPath(h, f, FALSE, k_exeFilter, NULL)) { Reveal_AddException(f); ApplyAll(FALSE); } break;
    case A_RV_CLEAR: g_cfg.rv_exceptions[0] = 0; ApplyAll(TRUE); break;
    case A_TWEAKS: Shell_ApplyWineTweaks(); MessageBoxW(h, L"Wine tweaks applied (title bars, focus, font smoothing, menu delay, hang timeouts).", APP_TITLE, MB_OK | MB_ICONINFORMATION); break;
    case A_BACKUP: lstrcpyW(f, L"MontyShell-backup.ini"); if (PickPath(h, f, TRUE, L"Settings backup\0*.ini\0", L"ini")) { if (!Settings_Backup(f)) MessageBoxW(h, L"Backup failed.", APP_TITLE, MB_OK | MB_ICONERROR); } break;
    case A_RESTORE: if (PickPath(h, f, FALSE, L"Settings backup\0*.ini\0All files\0*.*\0", NULL)) { if (Settings_Restore(f)) ApplyAll(FALSE); else MessageBoxW(h, L"Restore failed.", APP_TITLE, MB_OK | MB_ICONERROR); } break;
    case A_RESET: if (MessageBoxW(h, L"Reset ALL settings and app lists to defaults?", APP_TITLE, MB_YESNO | MB_ICONQUESTION) == IDYES) { Settings_Reset(); Img_Forget(); ApplyAll(TRUE); } break;
    case A_RESTART: Shell_PowerAction(2); break;
    case A_ABOUT: About_Show(); break;
    }
}

static void ChoiceMenu(HWND h, const Row *r, RECT fld)
{
    HMENU m = CreatePopupMenu();
    const wchar_t *o = OptsFor(r); wchar_t nm[64];
    for (int i = 0; ; i++) {
        OptName(o, i, nm, 64);
        if (!nm[0]) break;
        AppendMenuW(m, MF_STRING | (i == *IP(r) ? MF_CHECKED : 0) | (i && i % 14 == 0 ? MF_MENUBREAK : 0), 100 + i, nm);
    }
    POINT pt = { fld.left, fld.bottom }; ClientToScreen(h, &pt);
    int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, NULL);
    DestroyMenu(m);
    if (c >= 100) { *IP(r) = c - 100; ApplyAll(TRUE); }
}

static void SetSlider(const Row *r, RECT hit, int x)
{
    RECT tr = hit;
    int span = r->mx - r->mn, w = tr.right - tr.left;
    if (w < 1) return;
    int v = r->mn + (int)(((long long)(x - tr.left) * span + w / 2) / w);
    if (v < r->mn) v = r->mn; if (v > r->mx) v = r->mx;
    if (v != *IP(r)) { *IP(r) = v; ApplyAll(FALSE); }
}

static int HitAt(int x, int y)
{
    POINT p = { x, y };
    for (int i = g_nHits - 1; i >= 0; i--) if (PtInRect(&g_hits[i].r, p)) return i;
    return -1;
}

static void ClickRow(HWND h, const Row *r, RECT rr, int x, BOOL right)
{
    int cw = rr.right - rr.left;
    RECT ctl = { rr.left + cw * 54 / 100, rr.top, rr.right - 8, rr.bottom };
    switch (r->type) {
    case R_TOGGLE: *IP(r) = !*IP(r); ApplyAll(TRUE); break;
    case R_CHOICE: ChoiceMenu(h, r, (RECT){ ctl.left, ctl.top + 5, ctl.right, ctl.bottom - 5 }); break;
    case R_COLOR:
        if (right) { *IP(r) = -1; ApplyAll(TRUE); }
        else {
            static COLORREF custom[16]; CHOOSECOLORW cc; ZeroMemory(&cc, sizeof cc);
            cc.lStructSize = sizeof cc; cc.hwndOwner = h; cc.lpCustColors = custom; cc.Flags = CC_RGBINIT | CC_FULLOPEN;
            cc.rgbResult = *IP(r) == -1 ? g_pal.accent : (COLORREF)*IP(r);
            if (ChooseColorW(&cc)) { *IP(r) = (int)cc.rgbResult; ApplyAll(TRUE); }
        } break;
    case R_PATH:
        if (right) { SP(r)[0] = 0; Img_Forget(); ApplyAll(TRUE); }
        else { wchar_t f[MAX_PATH]; f[0] = 0; if (PickPath(h, f, FALSE, k_imgFilter, NULL)) { lstrcpynW(SP(r), f, 260); Img_Forget(); ApplyAll(TRUE); } }
        break;
    case R_TEXT: {
        CommitEdit();
        RECT fld = { ctl.left, ctl.top + 5, ctl.right, ctl.bottom - 5 };
        g_edit = CreateWindowExW(0, L"EDIT", SP(r), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, fld.left + 4, fld.top + 4, fld.right - fld.left - 8, fld.bottom - fld.top - 8, h, (HMENU)7001, g_hInst, NULL);
        SendMessageW(g_edit, WM_SETFONT, (WPARAM)Theme_Font(g_cfg.p_fontsize, FALSE), TRUE);
        g_editRow = r; SetFocus(g_edit); SendMessageW(g_edit, EM_SETSEL, 0, -1);
        break; }
    case R_ACTION: DoAction(h, r->id); break;
    }
    (void)x;
}

static void AppsClick(HWND h, const Hit *t)
{
    static const wchar_t *names[LIST_COUNT] = { L"Start app list", L"Pinned tiles", L"Taskbar shortcuts", L"Tray apps" };
    switch (t->kind) {
    case K_APPSEL: {
        HMENU m = CreatePopupMenu();
        for (int i = 0; i < LIST_COUNT; i++) AppendMenuW(m, MF_STRING | (i == g_appList ? MF_CHECKED : 0), 100 + i, names[i]);
        POINT pt = { t->r.left, t->r.bottom }; ClientToScreen(h, &pt);
        int c = TrackPopupMenu(m, TPM_RETURNCMD, pt.x, pt.y, 0, h, NULL); DestroyMenu(m);
        if (c >= 100) { g_appList = c - 100; g_scroll = 0; InvalidateRect(h, NULL, FALSE); }
        break; }
    case K_APPADD: { wchar_t f[MAX_PATH]; f[0] = 0; if (PickPath(h, f, FALSE, k_exeFilter, NULL)) { AppList_Add(g_appList, f); ApplyAll(TRUE); } break; }
    case K_APPSCAN: Shell_ScanStartMenu(g_appList); ApplyAll(TRUE); break;
    case K_APPDEL: AppList_Remove(g_appList, t->a); ApplyAll(TRUE); break;
    }
}

static LRESULT CALLBACK SetProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); Paint(dc); EndPaint(h, &ps); return 0; }
    case WM_SIZE: InvalidateRect(h, NULL, FALSE); return 0;
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp), n;
        if (g_sliderHit >= 0 && (wp & MK_LBUTTON)) {
            const Row *r = &g_tabs[g_tab].rows[g_hits[g_sliderHit].a];
            SetSlider(r, g_hits[g_sliderHit].r, x);
            return 0;
        }
        n = HitAt(x, (short)HIWORD(lp));
        if (n != g_hot) { g_hot = n; InvalidateRect(h, NULL, FALSE); }
        return 0; }
    case WM_LBUTTONDOWN: {
        CommitEdit();
        int n = HitAt((short)LOWORD(lp), (short)HIWORD(lp));
        if (n < 0) return 0;
        const Hit *t = &g_hits[n];
        if (t->kind == K_TAB) { g_tab = t->a; g_scroll = 0; InvalidateRect(h, NULL, FALSE); }
        else if (t->kind == K_ROW) {
            const Row *r = &g_tabs[g_tab].rows[t->a];
            if (r->type == R_SLIDER) { g_sliderHit = n; SetCapture(h); SetSlider(r, t->r, (short)LOWORD(lp)); }
            else ClickRow(h, r, t->r, (short)LOWORD(lp), FALSE);
        } else if (g_tab == TAB_APPS) AppsClick(h, t);
        return 0; }
    case WM_LBUTTONUP:
        if (g_sliderHit >= 0) { ReleaseCapture(); g_sliderHit = -1; Settings_Save(); }
        return 0;
    case WM_RBUTTONUP: {
        int n = HitAt((short)LOWORD(lp), (short)HIWORD(lp));
        if (n >= 0 && g_hits[n].kind == K_ROW) { const Row *r = &g_tabs[g_tab].rows[g_hits[n].a]; if (r->type == R_COLOR || r->type == R_PATH) ClickRow(h, r, g_hits[n].r, 0, TRUE); }
        return 0; }
    case WM_MOUSEWHEEL: {
        RECT rc; GetClientRect(h, &rc);
        int max = g_contentH - (rc.bottom - 16); if (max < 0) max = 0;
        g_scroll -= (short)HIWORD(wp) / WHEEL_DELTA * 50;
        if (g_scroll < 0) g_scroll = 0; if (g_scroll > max) g_scroll = max;
        InvalidateRect(h, NULL, FALSE); return 0; }
    case WM_COMMAND: if (LOWORD(wp) == 7001 && HIWORD(wp) == EN_KILLFOCUS) CommitEdit(); return 0;
    case WM_KEYDOWN: if (wp == VK_RETURN) CommitEdit(); return 0;
    case WM_CLOSE: CommitEdit(); Settings_Save(); ShowWindow(h, SW_HIDE); return 0;      /* keep it alive: reopens instantly */
    case WM_APP_REFRESH: InvalidateRect(h, NULL, FALSE); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

void SettingsUI_Show(int tab)
{
    if (!g_hSettings) {
        BuildOptionStrings();
        WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
        wc.lpfnWndProc = SetProc; wc.hInstance = g_hInst; wc.lpszClassName = L"MontySettings"; wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
        RegisterClassW(&wc);
        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
        int w = sw < 820 ? sw - 20 : 800, h = sh < 640 ? sh - 60 : 600;
        g_hSettings = CreateWindowExW(0, wc.lpszClassName, APP_TITLE L"  -  Settings", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      (sw - w) / 2, (sh - h) / 3, w, h, NULL, NULL, g_hInst, NULL);
    }
    if (tab >= 0 && tab < NTABS) { g_tab = tab; g_scroll = 0; }
    ShowWindow(g_hSettings, SW_SHOW);
    if (IsIconic(g_hSettings)) ShowWindow(g_hSettings, SW_RESTORE);
    SetForegroundWindow(g_hSettings);
    InvalidateRect(g_hSettings, NULL, FALSE);
}

/* ---------------- About / first-run welcome ---------------- */
#define GITHUB_URL L"https://github.com/monty786-dev/Winlator-Wine-Look-changer"
static HWND g_about; static int g_abHot = -1;

static void AboutBtns(RECT *a, RECT *b, RECT *c)
{
    RECT rc; GetClientRect(g_about, &rc);
    int bw = (rc.right - 48) / 3, y = rc.bottom - 56;
    *a = (RECT){ 16, y, 16 + bw, y + 38 }; *b = (RECT){ a->right + 8, y, a->right + 8 + bw, y + 38 }; *c = (RECT){ b->right + 8, y, rc.right - 16, y + 38 };
}

static LRESULT CALLBACK AboutProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC wdc = BeginPaint(h, &ps); Palette p; Theme_Resolve(&p);
        RECT rc; GetClientRect(h, &rc);
        HDC dc = CreateCompatibleDC(wdc); HBITMAP bm = CreateCompatibleBitmap(wdc, rc.right, rc.bottom); HGDIOBJ ob = SelectObject(dc, bm);
        Draw_Surface(dc, &rc, &p, p.panel, g_cfg.p_fx_install);
        RECT hd = { 0, 0, rc.right, 78 }; Draw_Gradient(dc, &hd, p.accent, Col_Blend(p.accent, p.bg, 55), FALSE);
        RECT t1 = { 20, 12, rc.right - 20, 44 }, t2 = { 20, 44, rc.right - 20, 70 };
        Draw_Text(dc, &t1, APP_TITLE, Col_Text(p.accent), Theme_Font(15, TRUE), DT_SINGLELINE | DT_VCENTER);
        Draw_Text(dc, &t2, L"Version " APP_VERSION L"  -  made for Winlator", Col_Text(p.accent), Theme_Font(9, FALSE), DT_SINGLELINE | DT_VCENTER);
        RECT body = { 20, 92, rc.right - 20, rc.bottom - 66 };
        Draw_Text(dc, &body, L"Custom taskbar, start panel, window title bars and desktop for Winlator's Wine.\n\n"
                              L"If you like it, please give the project a star on GitHub or support its development:\n" GITHUB_URL,
                  p.panelFg, Theme_Font(g_cfg.p_fontsize, FALSE), DT_WORDBREAK);
        RECT b[3]; AboutBtns(&b[0], &b[1], &b[2]);
        static const wchar_t *lab[3] = { L"Star on GitHub", L"Open settings", L"Close" };
        for (int i = 0; i < 3; i++) {
            Draw_Round(dc, &b[i], i == 0 ? p.accent : (g_abHot == i ? p.hover : Col_Blend(p.panel, p.panelFg, 12)), CLR_NONE, p.radius / 2);
            Draw_Text(dc, &b[i], lab[i], i == 0 ? Col_Text(p.accent) : p.panelFg, Theme_Font(g_cfg.p_fontsize, TRUE), DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        }
        BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, ob); DeleteObject(bm); DeleteDC(dc); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: { RECT b[3]; AboutBtns(&b[0], &b[1], &b[2]); POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
        int n = -1; for (int i = 0; i < 3; i++) if (PtInRect(&b[i], pt)) n = i;
        if (n != g_abHot) { g_abHot = n; InvalidateRect(h, NULL, FALSE); } return 0; }
    case WM_LBUTTONUP: { RECT b[3]; AboutBtns(&b[0], &b[1], &b[2]); POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
        if (PtInRect(&b[0], pt)) ShellExecuteW(NULL, L"open", GITHUB_URL, NULL, NULL, SW_SHOWNORMAL);
        else if (PtInRect(&b[1], pt)) { ShowWindow(h, SW_HIDE); SettingsUI_Show(0); }
        else if (PtInRect(&b[2], pt)) ShowWindow(h, SW_HIDE);
        return 0; }
    case WM_CLOSE: ShowWindow(h, SW_HIDE); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

void About_Show(void)
{
    if (!g_about) {
        WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
        wc.lpfnWndProc = AboutProc; wc.hInstance = g_hInst; wc.lpszClassName = L"MontyAbout"; wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN), w = sw < 520 ? sw - 20 : 500, h = 320;
        g_about = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"About", WS_CAPTION | WS_SYSMENU | WS_VISIBLE, (sw - w) / 2, (sh - h) / 3, w, h, NULL, NULL, g_hInst, NULL);
    }
    ShowWindow(g_about, SW_SHOW); SetForegroundWindow(g_about); InvalidateRect(g_about, NULL, FALSE);
}
