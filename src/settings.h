/* settings.h - every integer setting lives in ONE X-macro table.
 * Load / save / backup / restore / reset are generated from it, so adding a
 * new option is a single line here (plus a row in ui.c if it needs a control).
 * Value -1 for colour settings means "use the theme colour" (auto-adapt). */
#ifndef SETTINGS_H
#define SETTINGS_H
#include <windows.h>

#define SETTINGS_INT_LIST(X) \
 /* ---- Taskbar: layout ---- */ \
 X(tb_position,0)        /* 0 bottom 1 top 2 left 3 right */ \
 X(tb_zorder,0)          /* 0 always in front of windows, 1 behind */ \
 X(tb_align,0)           /* 0 left 1 middle 2 right (right = start button on right corner) */ \
 X(tb_height,0)          /* 0 = automatic (taken from the selected OS theme) */ \
 X(tb_autowidth,0)       /* shrink bar to fit running apps */ \
 X(tb_iconsize,24) \
 X(tb_showtitle,1) \
 X(tb_rounded,0) \
 X(tb_detach,0)          /* gap in px between bar and screen edge */ \
 X(tb_color,-1) \
 X(tb_underline,1) \
 X(tb_underline_h,3) \
 X(tb_divider,1) X(tb_divider_w,1) /* 0 thin 1 mid 2 bold */ \
 /* ---- Taskbar: visibility ---- */ \
 X(tb_hide_native,1) \
 X(tb_underline_adapt,1) \
 X(tb_fs_mode,0)         /* 0 keep visible 1 visible over app 2 floating icon 3 hide */ \
 X(tb_applies_to,2)      /* 0 fullscreen 1 maximized 2 both */ \
 X(tb_autostart,1) X(tb_start_delay,2) \
 X(tb_preview,1) X(tb_prev_w,220) X(tb_prev_h,130) \
 /* ---- Taskbar: elements ---- */ \
 X(tb_show_desktop,1) X(tb_clock,1) X(tb_clock_day,0) \
 X(tb_tray_battery,1) X(tb_tray_wifi,1) X(tb_tray_bt,1) X(tb_tray_taskmgr,1) \
 X(tb_tray_grid,0) \
 /* ---- Reveal bar ---- */ \
 X(rv_enabled,1) X(rv_color,-1) X(rv_pos,0) X(rv_align,1) X(rv_corner,0) \
 X(rv_opacity,200) X(rv_height,4) X(rv_width,240) X(rv_seconds,3) X(rv_anim,2) \
 X(rv_in_normal,0)       /* also work in normal mode (taskbar auto-hides, bar shows) */ \
 /* ---- Start button / panel ---- */ \
 X(st_size,1)            /* 0 normal 1 mid 2 large 3 fullscreen */ \
 X(st_btn_color,-1) X(st_btn_icon,0) \
 X(st_vbar_pos,0)        /* 0 left 1 right 2 top 3 bottom */ \
 X(st_vbar_bg,0) X(st_panel_bg,0) /* 0 theme colour 1 picture 2 transparent */ \
 X(st_alpha,235) \
 X(st_tile_colors,1) X(st_tile_alpha,255) X(st_tile_anim,1) \
 X(st_tile_size,84) X(st_tile_random,0) \
 X(st_collapse_list,0) X(st_collapse_tiles,0) \
 /* ---- Title bar ---- */ \
 X(tt_enabled,1) X(tt_theme,0) X(tt_alpha,255) X(tt_apply_all,0) \
 X(tt_hide_native,1) X(tt_btn_pin,1) X(tt_btn_shade,1) X(tt_height,28) X(tt_color,-1) \
 /* ---- Personalization ---- */ \
 X(p_theme,5)            /* index into g_themes[] (5 = Windows 10) */ \
 X(p_classic,0)          /* 1 = original exact look of that OS */ \
 X(p_effect,0)           /* 0 none 1 glass 2 iOS blur 3 3D 4 boxed 5 liquid glass */ \
 X(p_blur,8) X(p_accent,-1) X(p_shine,40) X(p_gloss,40) \
 X(p_shadow,1) X(p_round,1) \
 X(p_fx_taskbar,1) X(p_fx_start,1) X(p_fx_preview,1) X(p_fx_title,1) X(p_fx_install,1) \
 X(p_iconpreset,0) \
 X(p_mode,0)             /* 0 none 1 dark 2 light 3 amoled 4 rose 5 sapphire 6 aero */ \
 X(p_icon_recolor,0) X(p_icon_fill,0) \
 X(p_fontsize,9) X(p_uiscale,100) \
 /* ---- Desktop mode ---- */ \
 X(dk_enabled,0) X(dk_grid,84) X(dk_snap,1) X(dk_corner,0) X(dk_show_icons,1) \
 X(dk_widget_clock,1) X(dk_wall_fit,0) X(dk_lock,0) X(dk_wall_color,-1) \
 X(dk_fontsize,9) X(dk_uiscale,100) \
 /* ---- Efficiency (battery / CPU) ---- */ \
 X(perf_poll_ms,500) X(perf_idle_throttle,1) X(perf_battery_lite,1)

#define SETTINGS_STR_LIST(X) \
 X(user_name,64,L"User") \
 X(user_pic,260,L"") \
 X(start_icon_path,260,L"") \
 X(wallpaper,260,L"") \
 X(rv_exceptions,512,L"") \
 X(start_panel_pic,260,L"") \
 X(start_vbar_pic,260,L"") \
 X(font_name,32,L"Segoe UI")

typedef struct {
#define X(n,d) int n;
    SETTINGS_INT_LIST(X)
#undef X
#define X(n,len,d) wchar_t n[len];
    SETTINGS_STR_LIST(X)
#undef X
} Settings;

extern Settings g_cfg;

void Settings_Defaults(Settings *s);
void Settings_Load(void);
void Settings_Save(void);
BOOL Settings_Backup(const wchar_t *dst);   /* copy ini -> dst */
BOOL Settings_Restore(const wchar_t *src);  /* copy src -> ini, reload */
void Settings_Reset(void);
const wchar_t *Settings_Path(void);

/* app lists (start list / pinned tiles / taskbar shortcuts / tray) */
#define LIST_START   0
#define LIST_PINNED  1
#define LIST_TASKBAR 2
#define LIST_TRAY    3
#define LIST_COUNT   4
#define LIST_MAX     128
int  AppList_Count(int list);
const wchar_t *AppList_Get(int list, int i);          /* full path of exe/lnk */
BOOL AppList_Add(int list, const wchar_t *path);
void AppList_Remove(int list, int i);
void AppList_Move(int list, int from, int to);
void AppList_Load(void);
void AppList_Save(void);

#endif
