#include "app.h"
#include <tlhelp32.h>
#include <shlobj.h>
#include <shellapi.h>
#include <string.h>
#include <wchar.h>

/* ---------- process / window helpers ---------- */
BOOL Shell_GetExeName(HWND w, wchar_t *out, int n)
{
    DWORD pid = 0; out[0] = 0;
    GetWindowThreadProcessId(w, &pid);
    if (!pid) return FALSE;
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return FALSE;
    DWORD len = (DWORD)n;
    BOOL ok = QueryFullProcessImageNameW(h, 0, out, &len);
    CloseHandle(h);
    return ok;
}

static BOOL IsOwnWindow(HWND w)
{
    DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

BOOL Shell_IsWindowAppWindow(HWND w)
{
    if (!IsWindowVisible(w) || IsOwnWindow(w)) return FALSE;
    if (GetWindow(w, GW_OWNER)) return FALSE;
    LONG ex = GetWindowLongW(w, GWL_EXSTYLE);
    if ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW)) return FALSE;
    if (ex & WS_EX_NOACTIVATE) return FALSE;
    wchar_t cls[64]; GetClassNameW(w, cls, 64);
    if (!lstrcmpW(cls, L"Progman") || !lstrcmpW(cls, L"WorkerW") || !lstrcmpW(cls, L"Shell_TrayWnd") ||
        !lstrcmpW(cls, L"Shell_SecondaryTrayWnd") || !lstrcmpW(cls, L"MontyOverlay")) return FALSE;
    if (GetWindowTextLengthW(w) == 0) return FALSE;
    RECT r; GetWindowRect(w, &r);
    if (r.right - r.left < 2 || r.bottom - r.top < 2) return FALSE;
    return TRUE;
}

void Shell_Run(const wchar_t *path)
{
    if (!path || !path[0]) return;
    ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL);
}

/* ---------- hide Winlator's built-in taskbar (strip windows owned by wfm.exe) ---------- */
static DWORD g_wfmPids[8]; static int g_wfmN;

static BOOL CALLBACK HideWfmProc(HWND w, LPARAM hide)
{
    DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
    BOOL mine = FALSE;
    for (int i = 0; i < g_wfmN; i++) if (g_wfmPids[i] == pid) mine = TRUE;
    if (!mine) return TRUE;
    RECT r, scr; GetWindowRect(w, &r);
    scr.left = 0; scr.top = 0; scr.right = GetSystemMetrics(SM_CXSCREEN); scr.bottom = GetSystemMetrics(SM_CYSCREEN);
    int ww = r.right - r.left, hh = r.bottom - r.top;
    /* a bar = long thin strip hugging a screen edge; never touch the full-size desktop window */
    BOOL strip = (ww >= scr.right * 7 / 10 && hh <= 90) || (hh >= scr.bottom * 7 / 10 && ww <= 90);
    if (strip) ShowWindow(w, hide ? SW_HIDE : SW_SHOWNA);
    return TRUE;
}

void Shell_HideNativeTaskbar(BOOL hide)
{
    g_wfmN = 0;
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe; pe.dwSize = sizeof pe;
    if (Process32FirstW(s, &pe)) do {
        if (!lstrcmpiW(pe.szExeFile, L"wfm.exe") && g_wfmN < 8) g_wfmPids[g_wfmN++] = pe.th32ProcessID;
    } while (Process32NextW(s, &pe));
    CloseHandle(s);
    if (g_wfmN) EnumWindows(HideWfmProc, (LPARAM)hide);
}

/* ---------- power ---------- */
BOOL Power_OnBattery(void)
{
    SYSTEM_POWER_STATUS sp;
    if (!GetSystemPowerStatus(&sp)) return FALSE;
    return sp.ACLineStatus == 0;
}

void Shell_PowerAction(int what)
{
    switch (what) {
    case 0: LockWorkStation(); break;
    case 1: PostMessageW(g_hTaskbar, WM_CLOSE, 0, 0); break;                       /* exit shell */
    case 2: {                                                                      /* restart shell */
        wchar_t me[MAX_PATH]; GetModuleFileNameW(NULL, me, MAX_PATH);
        ShellExecuteW(NULL, L"open", me, L"--restart", NULL, SW_SHOWNORMAL);
        PostMessageW(g_hTaskbar, WM_CLOSE, 0, 0); break; }
    case 3: ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCEIFHUNG, 0); break;              /* stops this Wine container */
    }
}

/* ---------- Wine stability / performance registry values ---------- */
static void SetStr(HKEY root, const wchar_t *key, const wchar_t *name, const wchar_t *val)
{
    HKEY k;
    if (RegCreateKeyExW(root, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val, (lstrlenW(val) + 1) * sizeof(wchar_t));
        RegCloseKey(k);
    }
}

void Shell_ApplyWineTweaks(void)
{
    /* Wine draws NO native frame, so our custom title bar is the only one (no double title bars) */
    SetStr(HKEY_CURRENT_USER, L"Software\\Wine\\X11 Driver", L"Decorated", g_cfg.tt_hide_native ? L"N" : L"Y");
    /* fewer focus fights between Wine's window manager hooks and our overlay windows */
    SetStr(HKEY_CURRENT_USER, L"Software\\Wine\\X11 Driver", L"UseTakeFocus", L"N");
    /* snappier UI, cleaner text */
    SetStr(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"MenuShowDelay", L"0");
    SetStr(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"FontSmoothing", L"2");
    SetStr(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"FontSmoothingType", L"2");
    SetStr(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"DragFullWindows", L"1");
    SetStr(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"AutoEndTasks", L"1");
    SetStr(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"HungAppTimeout", L"3000");
    SetStr(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"WaitToKillAppTimeout", L"3000");
}

/* ---------- Start-menu scan (installed apps) ---------- */
static void ScanDir(const wchar_t *dir, int list, int depth)
{
    wchar_t pat[MAX_PATH], full[MAX_PATH];
    wsprintfW(pat, L"%s\\*", dir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == L'.') continue;
        wsprintfW(full, L"%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth < 4) ScanDir(full, list, depth + 1);
        } else {
            int n = lstrlenW(fd.cFileName);
            if (n > 4 && (!lstrcmpiW(fd.cFileName + n - 4, L".lnk") || !lstrcmpiW(fd.cFileName + n - 4, L".exe")))
                AppList_Add(list, full);
        }
    } while (FindNextFileW(h, &fd) && AppList_Count(list) < LIST_MAX);
    FindClose(h);
}

void Shell_ScanStartMenu(int list)
{
    wchar_t dir[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_COMMON_PROGRAMS, NULL, 0, dir) == S_OK) ScanDir(dir, list, 0);
    if (SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, dir) == S_OK)        ScanDir(dir, list, 0);
    /* a few always-useful system programs */
    wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
    static const wchar_t *sysApps[] = { L"notepad.exe", L"regedit.exe", L"taskmgr.exe", L"control.exe",
                                        L"winecfg.exe", L"cmd.exe", L"explorer.exe" };
    for (int i = 0; i < 7; i++) {
        wchar_t p[MAX_PATH]; wsprintfW(p, L"%s\\%s", sys, sysApps[i]);
        if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) AppList_Add(list, p);
    }
}

/* ---------- .lnk resolving & display names (plain C vtable calls, no COBJMACROS) ---------- */
#include <objbase.h>
#include <shobjidl.h>

BOOL Shell_ResolveTarget(const wchar_t *path, wchar_t *out)
{
    int n = lstrlenW(path);
    lstrcpyW(out, path);
    if (n < 5 || lstrcmpiW(path + n - 4, L".lnk")) return TRUE;      /* already an exe / file */

    BOOL ok = FALSE;
    HRESULT init = CoInitialize(NULL);
    IShellLinkW *sl = NULL;
    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_IShellLinkW, (void **)&sl))) {
        IPersistFile *pf = NULL;
        if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf))) {
            if (SUCCEEDED(pf->lpVtbl->Load(pf, path, STGM_READ))) {
                wchar_t t[MAX_PATH];
                if (SUCCEEDED(sl->lpVtbl->GetPath(sl, t, MAX_PATH, NULL, 0)) && t[0]) {
                    lstrcpyW(out, t);
                    ok = TRUE;
                }
            }
            pf->lpVtbl->Release(pf);
        }
        sl->lpVtbl->Release(sl);
    }
    if (SUCCEEDED(init)) CoUninitialize();
    return ok;
}

void Shell_DisplayName(const wchar_t *path, wchar_t *out, int n)
{
    const wchar_t *s = wcsrchr(path, L'\\');
    s = s ? s + 1 : path;
    lstrcpynW(out, s, n);
    wchar_t *dot = wcsrchr(out, L'.');
    if (dot) *dot = 0;
}

/* ---------- start with the container (HKCU Run) ---------- */
void Shell_SetAutostart(BOOL on)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t me[MAX_PATH + 16], cmd[MAX_PATH + 32];
        GetModuleFileNameW(NULL, me, MAX_PATH);
        wsprintfW(cmd, L"\"%s\" --boot", me);
        RegSetValueExW(k, L"MontyShell", 0, REG_SZ, (const BYTE *)cmd, (lstrlenW(cmd) + 1) * sizeof(wchar_t));
    } else RegDeleteValueW(k, L"MontyShell");
    RegCloseKey(k);
}
