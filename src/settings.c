#include "settings.h"
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

Settings g_cfg;

/* turn #n into a wide string literal (L#n is not valid C) */
#define WIDE2(x) L##x
#define WIDE(x)  WIDE2(x)

static wchar_t g_ini[MAX_PATH];
static wchar_t g_lists[LIST_COUNT][LIST_MAX][MAX_PATH];
static int     g_listN[LIST_COUNT];

static const wchar_t *k_listKey[LIST_COUNT] = { L"Start", L"Pinned", L"Taskbar", L"Tray" };

const wchar_t *Settings_Path(void)
{
    if (!g_ini[0]) {
        wchar_t dir[MAX_PATH];
        if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, dir) != S_OK)
            lstrcpyW(dir, L"C:\\");
        lstrcatW(dir, L"\\MontyShell");
        CreateDirectoryW(dir, NULL);
        wsprintfW(g_ini, L"%s\\settings.ini", dir);
    }
    return g_ini;
}

void Settings_Defaults(Settings *s)
{
#define X(n,d) s->n = d;
    SETTINGS_INT_LIST(X)
#undef X
#define X(n,len,d) lstrcpynW(s->n, d, len);
    SETTINGS_STR_LIST(X)
#undef X
}

void Settings_Load(void)
{
    const wchar_t *ini = Settings_Path();
    Settings_Defaults(&g_cfg);
#define X(n,d) { wchar_t tmp_[24]; GetPrivateProfileStringW(L"Settings", WIDE(#n), L"", tmp_, 24, ini); g_cfg.n = tmp_[0] ? (int)wcstol(tmp_, NULL, 10) : (d); }
    SETTINGS_INT_LIST(X)
#undef X
#define X(n,len,d) GetPrivateProfileStringW(L"Settings", WIDE(#n), d, g_cfg.n, len, ini);
    SETTINGS_STR_LIST(X)
#undef X
    /* GetPrivateProfileInt returns UINT, so -1 round-trips as 0xFFFFFFFF -> (int)-1: fine. */
    AppList_Load();
}

/* append "key=value\0" to a double-NUL terminated section block */
static BOOL Sec_Add(wchar_t *buf, size_t cap, size_t *len, const wchar_t *key, const wchar_t *val)
{
    size_t need = (size_t)lstrlenW(key) + 1 + (size_t)lstrlenW(val) + 1;
    if (*len + need + 1 > cap) return FALSE;
    lstrcpyW(buf + *len, key); *len += lstrlenW(key);
    buf[(*len)++] = L'=';
    lstrcpyW(buf + *len, val); *len += lstrlenW(val);
    buf[(*len)++] = 0;
    buf[*len] = 0;
    return TRUE;
}

void Settings_Save(void)
{
    const wchar_t *ini = Settings_Path();
    static wchar_t buf[24000];
    size_t len = 0; buf[0] = buf[1] = 0;
    wchar_t num[24];
#define X(n,d) wsprintfW(num, L"%d", g_cfg.n); Sec_Add(buf, 24000, &len, WIDE(#n), num);
    SETTINGS_INT_LIST(X)
#undef X
#define X(n,l,d) Sec_Add(buf, 24000, &len, WIDE(#n), g_cfg.n);
    SETTINGS_STR_LIST(X)
#undef X
    WritePrivateProfileSectionW(L"Settings", buf, ini);
    AppList_Save();
}

BOOL Settings_Backup(const wchar_t *dst)
{
    Settings_Save();
    return CopyFileW(Settings_Path(), dst, FALSE);
}

BOOL Settings_Restore(const wchar_t *src)
{
    if (!CopyFileW(src, Settings_Path(), FALSE)) return FALSE;
    Settings_Load();
    return TRUE;
}

void Settings_Reset(void)
{
    DeleteFileW(Settings_Path());
    memset(g_listN, 0, sizeof g_listN);
    Settings_Load();
}

/* ---------------- app lists ---------------- */
int AppList_Count(int l) { return (l >= 0 && l < LIST_COUNT) ? g_listN[l] : 0; }

const wchar_t *AppList_Get(int l, int i)
{
    if (l < 0 || l >= LIST_COUNT || i < 0 || i >= g_listN[l]) return L"";
    return g_lists[l][i];
}

BOOL AppList_Add(int l, const wchar_t *p)
{
    if (l < 0 || l >= LIST_COUNT || g_listN[l] >= LIST_MAX) return FALSE;
    for (int i = 0; i < g_listN[l]; i++)
        if (!lstrcmpiW(g_lists[l][i], p)) return FALSE;   /* no duplicates */
    lstrcpynW(g_lists[l][g_listN[l]++], p, MAX_PATH);
    return TRUE;
}

void AppList_Remove(int l, int i)
{
    if (l < 0 || l >= LIST_COUNT || i < 0 || i >= g_listN[l]) return;
    for (int k = i; k < g_listN[l] - 1; k++)
        lstrcpyW(g_lists[l][k], g_lists[l][k + 1]);
    g_listN[l]--;
}

void AppList_Move(int l, int from, int to)
{
    if (l < 0 || l >= LIST_COUNT) return;
    if (from < 0 || to < 0 || from >= g_listN[l] || to >= g_listN[l] || from == to) return;
    wchar_t tmp[MAX_PATH];
    lstrcpyW(tmp, g_lists[l][from]);
    if (from < to) for (int k = from; k < to; k++) lstrcpyW(g_lists[l][k], g_lists[l][k + 1]);
    else           for (int k = from; k > to; k--) lstrcpyW(g_lists[l][k], g_lists[l][k - 1]);
    lstrcpyW(g_lists[l][to], tmp);
}

/* each list lives in its own [List_<name>] section: Count=N, 0=path, 1=path ... */
void AppList_Load(void)
{
    const wchar_t *ini = Settings_Path();
    wchar_t sec[40], key[16];
    for (int l = 0; l < LIST_COUNT; l++) {
        g_listN[l] = 0;
        wsprintfW(sec, L"List_%s", k_listKey[l]);
        int n = (int)GetPrivateProfileIntW(sec, L"Count", 0, ini);
        if (n > LIST_MAX) n = LIST_MAX;
        for (int i = 0; i < n; i++) {
            wsprintfW(key, L"%d", i);
            GetPrivateProfileStringW(sec, key, L"", g_lists[l][g_listN[l]], MAX_PATH, ini);
            if (g_lists[l][g_listN[l]][0]) g_listN[l]++;
        }
    }
}

void AppList_Save(void)
{
    const wchar_t *ini = Settings_Path();
    static wchar_t buf[32000];
    wchar_t sec[40], key[16], num[16];
    for (int l = 0; l < LIST_COUNT; l++) {
        size_t len = 0; buf[0] = buf[1] = 0;
        int kept = 0;
        for (int i = 0; i < g_listN[l]; i++) {
            wsprintfW(key, L"%d", kept);
            if (!Sec_Add(buf, 32000, &len, key, g_lists[l][i])) break;      /* section size limit: keep what fits */
            kept++;
        }
        wsprintfW(num, L"%d", kept);
        Sec_Add(buf, 32000, &len, L"Count", num);
        wsprintfW(sec, L"List_%s", k_listKey[l]);
        WritePrivateProfileSectionW(sec, buf, ini);
    }
}
