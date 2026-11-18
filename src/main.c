/* main.c - entry point of "Taskbar & Desktop by Monty" (MontyShell.exe). */
#include "app.h"
#include <shellapi.h>
#include <objbase.h>

HINSTANCE g_hInst;

void App_ApplyAll(void)
{
    Shell_SetAutostart(g_cfg.tb_autostart);
    Theme_Resolve(&(Palette){ 0 });                 /* warm the font cache with current scale */
    Taskbar_Refresh();
    StartPanel_Refresh();
    Desktop_Refresh();
    TitleBar_Refresh();
    Reveal_Refresh();
    if (!g_cfg.tb_hide_native) Shell_HideNativeTaskbar(FALSE);
    if (g_hSettings) InvalidateRect(g_hSettings, NULL, FALSE);
}

static BOOL HasArg(const wchar_t *flag)
{
    int n = 0; BOOL found = FALSE;
    wchar_t **a = CommandLineToArgvW(GetCommandLineW(), &n);
    if (!a) return FALSE;
    for (int i = 1; i < n; i++) if (!lstrcmpiW(a[i], flag)) found = TRUE;
    LocalFree(a);
    return found;
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show)
{
    (void)hp; (void)cmd; (void)show;
    g_hInst = hi;

    /* single instance; after "--restart" wait a moment for the old copy to exit */
    HANDLE mx = CreateMutexW(NULL, TRUE, L"MontyShell.Singleton");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HasArg(L"--restart")) {
            for (int i = 0; i < 30 && WaitForSingleObject(mx, 100) != WAIT_OBJECT_0; i++) ;
        } else { CloseHandle(mx); return 0; }
    }

    CoInitialize(NULL);
    Settings_Load();

    /* started by the container at boot: honour the startup delay so Wine's services come up first */
    if (HasArg(L"--boot") && g_cfg.tb_start_delay > 0) Sleep((DWORD)g_cfg.tb_start_delay * 1000);

    BOOL first = GetPrivateProfileIntW(L"App", L"FirstRun", 1, Settings_Path()) != 0;

    Shell_ApplyWineTweaks();
    if (!Taskbar_Create()) { MessageBoxW(NULL, L"Could not create the taskbar window.", APP_TITLE, MB_OK | MB_ICONERROR); return 1; }
    StartPanel_Create();
    Reveal_Create();
    TitleBar_Init();
    Desktop_Create();
    App_ApplyAll();

    if (first) {                                           /* first run: show Info / About with the GitHub star link */
        WritePrivateProfileStringW(L"App", L"FirstRun", L"0", Settings_Path());
        Settings_Save();
        About_Show();
    }
    if (HasArg(L"--settings")) SettingsUI_Show(0);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    Settings_Save();
    CoUninitialize();
    ReleaseMutex(mx); CloseHandle(mx);
    return 0;
}
