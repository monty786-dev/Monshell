/* setup.c - installer / uninstaller for "Taskbar & Desktop by Monty".
 * MontyShell.exe is embedded as resource #1 (RCDATA). Same GDI look as the shell, no extra dependencies.
 * Build: see build.sh / the GitHub workflow. Run with --uninstall (done by Uninstall.exe) to remove. */
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <wchar.h>
#include "../src/settings.h"
#include "../src/themes.h"
#include "../src/draw.h"

#define APP_TITLE   L"Taskbar & Desktop by Monty"
#define GITHUB_URL  L"https://github.com/monty786-dev/Winlator-Wine-Look-changer"
#define W 560
#define H 400

enum { P_WELCOME, P_WORKING, P_DONE };
enum { B_INSTALL = 1, B_CANCEL, B_STAR, B_FINISH, B_OPT0 = 10 };

typedef struct { const wchar_t *label, *hint; int on; } Opt;
static Opt g_opt[] = {
    { L"Start automatically with the container", L"adds a Run entry (HKCU)", 1 },
    { L"Make it Winlator's default shell (replace wfm.exe)", L"original is kept as wfm.exe.orig and restored on uninstall", 1 },
    { L"Apply Wine stability / performance tweaks", L"no native frame, faster menus, font smoothing, shorter hang timeouts", 1 },
    { L"Start menu shortcuts", L"Taskbar & Desktop, Settings, Uninstall", 1 },
    { L"Desktop shortcut", L"", 0 },
};
#define NOPT 5

static HWND g_hw;
static int  g_page = P_WELCOME, g_uninstall, g_progress, g_hot = -1, g_startNow = 1, g_fail;
static wchar_t g_dir[MAX_PATH], g_log[12][120]; static int g_logN;
static Palette g_pal;
typedef struct { int id; RECT r; } Hit;
static Hit g_hits[24]; static int g_nHits;

/* ------------------------------------------------------------------ helpers */
static void AddHit(int id, RECT r) { if (g_nHits < 24) { g_hits[g_nHits].id = id; g_hits[g_nHits].r = r; g_nHits++; } }

static void Log(const wchar_t *s)
{
    if (g_logN == 12) { for (int i = 1; i < 12; i++) lstrcpyW(g_log[i - 1], g_log[i]); g_logN = 11; }
    lstrcpynW(g_log[g_logN++], s, 120);
    InvalidateRect(g_hw, NULL, FALSE); UpdateWindow(g_hw);
}

static void Step(int pct, const wchar_t *msg, BOOL ok)
{
    wchar_t b[140]; wsprintfW(b, L"%s  %s", ok ? L"[ OK ]" : L"[FAIL]", msg);
    if (!ok) g_fail++;
    g_progress = pct;
    Log(b);
}

static void RegStr(const wchar_t *key, const wchar_t *name, const wchar_t *val)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val, (DWORD)((lstrlenW(val) + 1) * sizeof(wchar_t)));
        RegCloseKey(k);
    }
}
static void RegDword(const wchar_t *key, const wchar_t *name, DWORD v)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE *)&v, sizeof v);
        RegCloseKey(k);
    }
}
static BOOL RegRead(const wchar_t *key, const wchar_t *name, wchar_t *out, DWORD n)
{
    HKEY k; DWORD type = 0, sz = n * sizeof(wchar_t); BOOL ok = FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
        ok = RegQueryValueExW(k, name, NULL, &type, (BYTE *)out, &sz) == ERROR_SUCCESS && type == REG_SZ;
        RegCloseKey(k);
    }
    return ok;
}

static BOOL MakeLink(const wchar_t *lnk, const wchar_t *target, const wchar_t *args)
{
    BOOL ok = FALSE; IShellLinkW *sl = NULL;
    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl))) {
        IPersistFile *pf = NULL;
        sl->lpVtbl->SetPath(sl, target);
        if (args) sl->lpVtbl->SetArguments(sl, args);
        sl->lpVtbl->SetWorkingDirectory(sl, g_dir);
        sl->lpVtbl->SetIconLocation(sl, target, 0);
        if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf))) {
            ok = SUCCEEDED(pf->lpVtbl->Save(pf, lnk, TRUE));
            pf->lpVtbl->Release(pf);
        }
        sl->lpVtbl->Release(sl);
    }
    return ok;
}

static void StopShell(void)                      /* ask a running MontyShell to exit, wait up to 3 s */
{
    HWND w = FindWindowW(L"MontyTaskbar", NULL);
    if (!w) return;
    PostMessageW(w, WM_CLOSE, 0, 0);
    for (int i = 0; i < 30 && IsWindow(w); i++) Sleep(100);
}

static BOOL ExtractShell(const wchar_t *dst)
{
    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(1), (LPCWSTR)RT_RCDATA);
    if (!r) return FALSE;
    HGLOBAL g = LoadResource(NULL, r); DWORD sz = SizeofResource(NULL, r); const void *p = g ? LockResource(g) : NULL;
    if (!p || !sz) return FALSE;
    wchar_t old[MAX_PATH]; wsprintfW(old, L"%s.old", dst);
    DeleteFileW(old);
    HANDLE f = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {                                  /* in use: move it aside and retry */
        MoveFileW(dst, old);
        f = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f == INVALID_HANDLE_VALUE) return FALSE;
    }
    DWORD wr = 0; BOOL ok = WriteFile(f, p, sz, &wr, NULL) && wr == sz;
    CloseHandle(f);
    return ok;
}

static BOOL FindWfm(wchar_t *out)
{
    wchar_t d[MAX_PATH];
    GetWindowsDirectoryW(d, MAX_PATH); wsprintfW(out, L"%s\\wfm.exe", d);
    if (GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES) return TRUE;
    GetSystemDirectoryW(d, MAX_PATH); wsprintfW(out, L"%s\\wfm.exe", d);
    return GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES;
}

static void ProgDir(const wchar_t *name, wchar_t *out)
{
    wchar_t d[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, d) != S_OK) lstrcpyW(d, L"C:\\Windows");
    wsprintfW(out, L"%s\\%s", d, name);
}

/* ------------------------------------------------------------------ install */
static void DoInstall(void)
{
    wchar_t exe[MAX_PATH], tmp[MAX_PATH], tmp2[MAX_PATH], self[MAX_PATH];
    wsprintfW(exe, L"%s\\MontyShell.exe", g_dir);
    GetModuleFileNameW(NULL, self, MAX_PATH);

    StopShell();
    Step(8, L"Stopped running shell (if any)", TRUE);

    CreateDirectoryW(g_dir, NULL);
    BOOL ok = GetFileAttributesW(g_dir) != INVALID_FILE_ATTRIBUTES;
    Step(15, L"Created install folder", ok);
    if (!ok) return;

    Step(35, L"Installed MontyShell.exe", ExtractShell(exe));
    wsprintfW(tmp, L"%s\\Uninstall.exe", g_dir);
    Step(42, L"Installed uninstaller", CopyFileW(self, tmp, FALSE));

    RegStr(L"Software\\MontyShell", L"InstallDir", g_dir);

    if (g_opt[3].on) {
        wchar_t dir[MAX_PATH]; ProgDir(L"Taskbar & Desktop by Monty", dir);
        CreateDirectoryW(dir, NULL);
        wsprintfW(tmp, L"%s\\Taskbar & Desktop.lnk", dir);
        BOOL a = MakeLink(tmp, exe, NULL);
        wsprintfW(tmp, L"%s\\Settings.lnk", dir);
        BOOL b = MakeLink(tmp, exe, L"--settings");
        wsprintfW(tmp2, L"%s\\Uninstall.exe", g_dir); wsprintfW(tmp, L"%s\\Uninstall.lnk", dir);
        BOOL c = MakeLink(tmp, tmp2, L"--uninstall");
        Step(55, L"Start menu shortcuts", a && b && c);
    }
    if (g_opt[4].on) {
        wchar_t d[MAX_PATH];
        BOOL ok2 = SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, d) == S_OK;
        if (ok2) { wsprintfW(tmp, L"%s\\Taskbar & Desktop.lnk", d); ok2 = MakeLink(tmp, exe, NULL); }
        Step(62, L"Desktop shortcut", ok2);
    }
    if (g_opt[0].on) {
        wsprintfW(tmp, L"\"%s\" --boot", exe);
        RegStr(L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", L"MontyShell", tmp);
        Step(70, L"Autostart entry added", TRUE);
    }
    if (g_opt[1].on) {
        wchar_t wfm[MAX_PATH], orig[MAX_PATH];
        if (!FindWfm(wfm)) Step(82, L"wfm.exe not found in this container - skipped (autostart is used instead)", TRUE);
        else {
            wsprintfW(orig, L"%s.orig", wfm);
            if (GetFileAttributesW(orig) == INVALID_FILE_ATTRIBUTES) MoveFileW(wfm, orig);   /* keep the original */
            else DeleteFileW(wfm);
            BOOL r = CopyFileW(exe, wfm, FALSE);
            if (r) RegStr(L"Software\\MontyShell", L"WfmPath", wfm);
            else if (GetFileAttributesW(wfm) == INVALID_FILE_ATTRIBUTES && GetFileAttributesW(orig) != INVALID_FILE_ATTRIBUTES)
                MoveFileW(orig, wfm);                                                      /* roll back */
            Step(82, r ? L"wfm.exe replaced (backup: wfm.exe.orig)" : L"Could not replace wfm.exe (in use). Restart the container and run setup again", r);
        }
    }
    if (g_opt[2].on) {
        const wchar_t *x = L"Software\\Wine\\X11 Driver", *d = L"Control Panel\\Desktop";
        RegStr(x, L"Decorated", L"N");           RegStr(x, L"UseTakeFocus", L"N");
        RegDword(L"Software\\Wine\\WineDbg", L"ShowCrashDialog", 0);
        RegStr(d, L"MenuShowDelay", L"0");       RegStr(d, L"FontSmoothing", L"2");
        RegStr(d, L"FontSmoothingType", L"2");   RegStr(d, L"DragFullWindows", L"1");
        RegStr(d, L"AutoEndTasks", L"1");        RegStr(d, L"HungAppTimeout", L"3000");
        RegStr(d, L"WaitToKillAppTimeout", L"3000");
        Step(92, L"Wine stability / performance tweaks applied", TRUE);
    }
    g_progress = 100;
    Log(g_fail ? L"Finished with problems - see the lines marked FAIL." : L"All done.");
}

/* ------------------------------------------------------------------ uninstall */
static void DoUninstall(void)
{
    wchar_t tmp[MAX_PATH], wfm[MAX_PATH], orig[MAX_PATH], dir[MAX_PATH], self[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    StopShell();
    Step(15, L"Stopped running shell", TRUE);

    if (RegRead(L"Software\\MontyShell", L"WfmPath", wfm, MAX_PATH)) {
        wsprintfW(orig, L"%s.orig", wfm);
        if (GetFileAttributesW(orig) != INVALID_FILE_ATTRIBUTES) {
            DeleteFileW(wfm);
            BOOL r = MoveFileW(orig, wfm);
            Step(40, r ? L"Restored Winlator's original wfm.exe" : L"Could not restore wfm.exe", r);
        }
    }
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        RegDeleteValueW(k, L"MontyShell"); RegCloseKey(k);
    }
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\MontyShell");
    Step(55, L"Removed registry entries", TRUE);

    ProgDir(L"Taskbar & Desktop by Monty", dir);
    wsprintfW(tmp, L"%s\\Taskbar & Desktop.lnk", dir); DeleteFileW(tmp);
    wsprintfW(tmp, L"%s\\Settings.lnk", dir);           DeleteFileW(tmp);
    wsprintfW(tmp, L"%s\\Uninstall.lnk", dir);          DeleteFileW(tmp);
    RemoveDirectoryW(dir);
    if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, tmp) == S_OK) { lstrcatW(tmp, L"\\Taskbar & Desktop.lnk"); DeleteFileW(tmp); }
    Step(70, L"Removed shortcuts", TRUE);

    wsprintfW(tmp, L"%s\\MontyShell.exe", g_dir);     DeleteFileW(tmp);
    wsprintfW(tmp, L"%s\\MontyShell.exe.old", g_dir); DeleteFileW(tmp);
    Step(85, L"Removed program files", TRUE);

    /* this very exe is running: let a hidden cmd delete it (and the folder) a moment after we exit */
    wchar_t cmd[2 * MAX_PATH + 80];
    wsprintfW(cmd, L"cmd.exe /c ping -n 2 127.0.0.1 >nul & del /f /q \"%s\" & rmdir \"%s\"", self, g_dir);
    STARTUPINFOW si; PROCESS_INFORMATION pi; ZeroMemory(&si, sizeof si); si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    g_progress = 100;
    Log(L"Uninstalled. Your settings in %APPDATA%\\MontyShell were kept.");
}

/* ------------------------------------------------------------------ UI */
static void Button(HDC dc, RECT r, const wchar_t *t, int id, BOOL primary)
{
    AddHit(id, r);
    BOOL hot = g_hot == g_nHits - 1;
    COLORREF fill = primary ? (hot ? Col_Blend(g_pal.accent, RGB(255,255,255), 15) : g_pal.accent) : (hot ? g_pal.hover : Col_Blend(g_pal.panel, g_pal.panelFg, 12));
    Draw_Round(dc, &r, fill, CLR_NONE, 4);
    Draw_Text(dc, &r, t, primary ? Col_Text(g_pal.accent) : g_pal.panelFg, Theme_Font(10, TRUE), DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

static void Paint(HDC wdc)
{
    RECT rc; GetClientRect(g_hw, &rc);
    HDC dc = CreateCompatibleDC(wdc); HBITMAP bm = CreateCompatibleBitmap(wdc, rc.right, rc.bottom); HGDIOBJ ob = SelectObject(dc, bm);
    g_nHits = 0;
    Theme_Resolve(&g_pal);
    COLORREF fg = g_pal.panelFg, dim = g_pal.fgDim;
    Draw_Fill(dc, &rc, g_pal.panel);

    RECT hd = { 0, 0, rc.right, 78 };
    Draw_Gradient(dc, &hd, g_pal.accent, Col_Blend(g_pal.accent, g_pal.bg, 60), FALSE);
    RECT t1 = { 22, 12, rc.right - 20, 46 }, t2 = { 22, 46, rc.right - 20, 72 };
    Draw_Text(dc, &t1, APP_TITLE, Col_Text(g_pal.accent), Theme_Font(16, TRUE), DT_SINGLELINE | DT_VCENTER);
    Draw_Text(dc, &t2, g_uninstall ? L"Uninstall" : L"Setup  -  custom taskbar, start panel, title bars and desktop for Winlator",
              Col_Text(g_pal.accent), Theme_Font(9, FALSE), DT_SINGLELINE | DT_VCENTER);

    int by = rc.bottom - 56;
    if (g_page == P_WELCOME && !g_uninstall) {
        RECT intro = { 22, 92, rc.right - 22, 116 };
        Draw_Text(dc, &intro, L"Choose what to set up in this Winlator container:", fg, Theme_Font(10, TRUE), DT_SINGLELINE | DT_VCENTER);
        for (int i = 0; i < NOPT; i++) {
            RECT row = { 22, 124 + i * 40, rc.right - 22, 124 + i * 40 + 38 };
            AddHit(B_OPT0 + i, row);
            if (g_hot == g_nHits - 1) Draw_Round(dc, &row, Col_Blend(g_pal.panel, fg, 7), CLR_NONE, 4);
            RECT box = { row.left + 6, row.top + 9, row.left + 26, row.top + 29 };
            Draw_Round(dc, &box, g_opt[i].on ? g_pal.accent : g_pal.panel, g_opt[i].on ? g_pal.accent : dim, 3);
            if (g_opt[i].on) {
                HPEN p = CreatePen(PS_SOLID, 2, Col_Text(g_pal.accent)); HGDIOBJ o = SelectObject(dc, p);
                MoveToEx(dc, box.left + 4, box.top + 10, NULL); LineTo(dc, box.left + 8, box.top + 14); LineTo(dc, box.left + 16, box.top + 5);
                SelectObject(dc, o); DeleteObject(p);
            }
            RECT l1 = { row.left + 38, row.top + 1, row.right - 6, row.top + 20 }, l2 = { row.left + 38, row.top + 20, row.right - 6, row.top + 37 };
            Draw_Text(dc, &l1, g_opt[i].label, fg, Theme_Font(10, FALSE), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            if (g_opt[i].hint[0]) Draw_Text(dc, &l2, g_opt[i].hint, dim, Theme_Font(8, FALSE), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        RECT dr = { 22, 328, rc.right - 22, 346 };
        wchar_t s[MAX_PATH + 20]; wsprintfW(s, L"Install to: %s", g_dir);
        Draw_Text(dc, &dr, s, dim, Theme_Font(8, FALSE), DT_SINGLELINE | DT_VCENTER | DT_PATH_ELLIPSIS);
        Button(dc, (RECT){ rc.right - 22 - 110, by, rc.right - 22, by + 38 }, L"Install", B_INSTALL, TRUE);
        Button(dc, (RECT){ rc.right - 22 - 230, by, rc.right - 22 - 120, by + 38 }, L"Cancel", B_CANCEL, FALSE);
    } else if (g_page == P_WELCOME && g_uninstall) {
        RECT m = { 22, 100, rc.right - 22, 200 };
        Draw_Text(dc, &m, L"This removes Taskbar & Desktop by Monty from this container, restores Winlator's original wfm.exe (if it was replaced) and removes the autostart entry.\n\nYour settings are kept.",
                  fg, Theme_Font(10, FALSE), DT_WORDBREAK);
        Button(dc, (RECT){ rc.right - 22 - 130, by, rc.right - 22, by + 38 }, L"Uninstall", B_INSTALL, TRUE);
        Button(dc, (RECT){ rc.right - 22 - 250, by, rc.right - 22 - 140, by + 38 }, L"Cancel", B_CANCEL, FALSE);
    } else {                                                    /* working / done: progress + log */
        RECT pb = { 22, 98, rc.right - 22, 114 };
        Draw_Round(dc, &pb, Col_Blend(g_pal.panel, fg, 14), CLR_NONE, 8);
        RECT pf = pb; pf.right = pb.left + (pb.right - pb.left) * g_progress / 100;
        if (pf.right > pf.left + 8) Draw_Round(dc, &pf, g_pal.accent, CLR_NONE, 8);
        for (int i = 0; i < g_logN; i++) {
            RECT l = { 22, 128 + i * 20, rc.right - 22, 148 + i * 20 };
            BOOL bad = wcsstr(g_log[i], L"[FAIL]") != NULL;
            Draw_Text(dc, &l, g_log[i], bad ? RGB(232, 90, 90) : fg, Theme_Font(9, FALSE), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        if (g_page == P_DONE) {
            if (!g_uninstall) {
                RECT row = { 22, by, 250, by + 38 };
                AddHit(B_OPT0 + 20, row);
                RECT box = { row.left + 2, row.top + 9, row.left + 22, row.top + 29 };
                Draw_Round(dc, &box, g_startNow ? g_pal.accent : g_pal.panel, g_startNow ? g_pal.accent : dim, 3);
                if (g_startNow) {
                    HPEN p = CreatePen(PS_SOLID, 2, Col_Text(g_pal.accent)); HGDIOBJ o = SelectObject(dc, p);
                    MoveToEx(dc, box.left + 4, box.top + 10, NULL); LineTo(dc, box.left + 8, box.top + 14); LineTo(dc, box.left + 16, box.top + 5);
                    SelectObject(dc, o); DeleteObject(p);
                }
                RECT tt = { row.left + 32, row.top, row.right, row.bottom };
                Draw_Text(dc, &tt, L"Start it now", fg, Theme_Font(10, FALSE), DT_SINGLELINE | DT_VCENTER);
            }
            Button(dc, (RECT){ rc.right - 22 - 100, by, rc.right - 22, by + 38 }, L"Finish", B_FINISH, TRUE);
            Button(dc, (RECT){ rc.right - 22 - 270, by, rc.right - 22 - 110, by + 38 }, L"Star on GitHub", B_STAR, FALSE);
        }
    }
    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob); DeleteObject(bm); DeleteDC(dc);
}

static int HitAt(int x, int y)
{
    POINT p = { x, y };
    for (int i = g_nHits - 1; i >= 0; i--) if (PtInRect(&g_hits[i].r, p)) return i;
    return -1;
}

static void Click(int id)
{
    if (id >= B_OPT0 && id < B_OPT0 + NOPT) { g_opt[id - B_OPT0].on = !g_opt[id - B_OPT0].on; return; }
    switch (id) {
    case B_OPT0 + 20: g_startNow = !g_startNow; break;
    case B_CANCEL: DestroyWindow(g_hw); break;
    case B_STAR: ShellExecuteW(NULL, L"open", GITHUB_URL, NULL, NULL, SW_SHOWNORMAL); break;
    case B_FINISH:
        if (g_page == P_DONE && !g_uninstall && g_startNow && !g_fail) {
            wchar_t exe[MAX_PATH]; wsprintfW(exe, L"%s\\MontyShell.exe", g_dir);
            ShellExecuteW(NULL, L"open", exe, NULL, g_dir, SW_SHOWNORMAL);
        }
        DestroyWindow(g_hw); break;
    case B_INSTALL:
        g_page = P_WORKING; g_logN = 0; g_progress = 0; g_fail = 0;
        InvalidateRect(g_hw, NULL, FALSE); UpdateWindow(g_hw);
        if (g_uninstall) DoUninstall(); else DoInstall();
        g_page = P_DONE;
        break;
    }
}

static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); Paint(dc); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: { int n = HitAt((short)LOWORD(lp), (short)HIWORD(lp)); if (n != g_hot) { g_hot = n; InvalidateRect(h, NULL, FALSE); } return 0; }
    case WM_LBUTTONUP: {
        if (g_page == P_WORKING) return 0;
        int n = HitAt((short)LOWORD(lp), (short)HIWORD(lp));
        if (n >= 0) { int id = g_hits[n].id; Click(id); if (IsWindow(h)) InvalidateRect(h, NULL, FALSE); }
        return 0; }
    case WM_CLOSE: if (g_page != P_WORKING) DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show)
{
    (void)hp; (void)cmd; (void)show;
    int n = 0;
    LPWSTR *a = CommandLineToArgvW(GetCommandLineW(), &n);
    for (int i = 1; a && i < n; i++) if (!lstrcmpiW(a[i], L"--uninstall")) g_uninstall = 1;
    if (a) LocalFree(a);

    CoInitialize(NULL);
    Settings_Defaults(&g_cfg);                                      /* installer uses the default (Windows 10 dark) look */

    wchar_t pf[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, pf) != S_OK) lstrcpyW(pf, L"C:\\Program Files");
    wsprintfW(g_dir, L"%s\\MontyShell", pf);
    if (g_uninstall) {                                              /* uninstall from wherever Uninstall.exe lives */
        wchar_t self[MAX_PATH]; GetModuleFileNameW(NULL, self, MAX_PATH);
        wchar_t *s = wcsrchr(self, L'\\'); if (s) { *s = 0; lstrcpyW(g_dir, self); }
    }

    WNDCLASSW wc; ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = Proc; wc.hInstance = hi; wc.lpszClassName = L"MontySetup"; wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassW(&wc);
    RECT r = { 0, 0, W, H };
    AdjustWindowRect(&r, WS_CAPTION | WS_SYSMENU, FALSE);
    int ww = r.right - r.left, wh = r.bottom - r.top, sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    g_hw = CreateWindowExW(0, wc.lpszClassName, g_uninstall ? L"Uninstall - Taskbar & Desktop by Monty" : L"Setup - Taskbar & Desktop by Monty",
                           WS_CAPTION | WS_SYSMENU | WS_VISIBLE, (sw - ww) / 2, (sh - wh) / 2, ww, wh, NULL, NULL, hi, NULL);
    if (!g_hw) return 1;
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    CoUninitialize();
    return 0;
}
