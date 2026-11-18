/* app.h - cross-module API */
#ifndef APP_H
#define APP_H
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wchar.h>
#include "settings.h"
#include "themes.h"
#include "draw.h"

#define APP_TITLE   L"Taskbar & Desktop by Monty"
#define APP_VERSION L"1.0.0"
#define WM_APP_REFRESH  (WM_APP + 1)     /* settings changed: re-read g_cfg and repaint */
#define WM_APP_TRAY     (WM_APP + 2)

extern HINSTANCE g_hInst;
extern HWND g_hTaskbar, g_hStart, g_hReveal, g_hSettings;

/* taskbar.c */
BOOL Taskbar_Create(void);
void Taskbar_Relayout(void);                 /* position / size from settings */
void Taskbar_Show(BOOL show);
void Taskbar_Peek(void);                     /* bring up over fullscreen app, auto-hides */
BOOL Taskbar_IsHidden(void);
void Taskbar_Refresh(void);
void Taskbar_GetBarRect(RECT *r);
void Taskbar_SetFloating(BOOL f);            /* collapse to a floating start icon */
int  Taskbar_Thickness(void);                /* current bar thickness in px */

/* preview.c : hover live preview + tray grid popup */
void Preview_Show(HWND target, const RECT *anchorScreen);
void Preview_Hide(void);
BOOL Preview_IsVisible(void);
void TrayGrid_Toggle(const RECT *anchorScreen);
void TrayGrid_Hide(void);

/* startpanel.c */
BOOL StartPanel_Create(void);
void StartPanel_Toggle(void);
void StartPanel_Hide(void);
void StartPanel_Refresh(void);

/* reveal.c */
BOOL Reveal_Create(void);
void Reveal_Refresh(void);
void Reveal_Tick(void);
BOOL Reveal_IsFullscreenApp(HWND *app);      /* foreground window covers the monitor / is maximized */
BOOL Reveal_IsException(const wchar_t *exe);
void Reveal_AddException(const wchar_t *exe);

/* titlebar.c */
BOOL TitleBar_Init(void);
void TitleBar_Tick(void);
void TitleBar_Refresh(void);

/* desktop.c */
BOOL Desktop_Create(void);
void Desktop_Refresh(void);

/* ui.c (settings window) */
void SettingsUI_Show(int tab);
void About_Show(void);

/* main.c: re-apply every setting to every module (saves nothing by itself) */
void App_ApplyAll(void);
void Shell_SetAutostart(BOOL on);            /* HKCU Run entry so the shell starts with the container */

/* shell.c */
void Shell_HideNativeTaskbar(BOOL hide);
void Shell_Run(const wchar_t *path);
BOOL Shell_IsWindowAppWindow(HWND w);        /* normal top-level app window that deserves a button */
BOOL Shell_GetExeName(HWND w, wchar_t *out, int n);
void Shell_ApplyWineTweaks(void);            /* stability / perf registry values */
void Shell_PowerAction(int what);            /* 0 lock-ish 1 exit shell 2 restart shell 3 shutdown wineserver */
void Shell_ScanStartMenu(int list);          /* fill LIST_START from Start Menu folders */
BOOL Shell_ResolveTarget(const wchar_t *path, wchar_t *out); /* .lnk -> exe path (or same path) */
void Shell_DisplayName(const wchar_t *path, wchar_t *out, int n); /* file name without extension */

/* power helper (battery) */
BOOL Power_OnBattery(void);

#endif
