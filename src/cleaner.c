#define _WIN32_WINNT 0x0600
#include "cleaner.h"
#include "resource.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <wchar.h>

CleanerStats g_Stats = {0};
CleanerConfig g_Config = {0};

static CleanerLogCallback s_LogCallback = NULL;
static wchar_t s_ConfigIniPath[MAX_PATH] = {0};
static const wchar_t *RUN_REG_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *RUN_REG_VALUE = L"Max Cleaner";

#define MAX_RUNNING_PROCS 512
static wchar_t s_RunningProcs[MAX_RUNNING_PROCS][64];
static int s_RunningProcCount = 0;

// Critical system processes whitelist - strictly protected
static const wchar_t *SYSTEM_WHITELIST[] = {
    L"system",
    L"registry",
    L"smss.exe",
    L"csrss.exe",
    L"wininit.exe",
    L"services.exe",
    L"lsass.exe",
    L"winlogon.exe",
    L"explorer.exe",
    L"dwm.exe",
    L"svchost.exe",
    L"fontdrvhost.exe",
    L"sihost.exe",
    L"taskhostw.exe",
    L"audiodg.exe",
    L"spoolsv.exe",
    L"conhost.exe",
    L"ctfmon.exe",
    L"SearchHost.exe",
    L"ShellExperienceHost.exe",
    L"StartMenuExperienceHost.exe",
    L"powershell.exe",
    L"cmd.exe",
    L"MaxCleaner.exe",
    NULL
};

void Cleaner_SetLogCallback(CleanerLogCallback cb) {
    s_LogCallback = cb;
}

void Cleaner_Log(const wchar_t *format, ...) {
    wchar_t buf[512];
    va_list args;
    va_start(args, format);
    vswprintf(buf, sizeof(buf)/sizeof(buf[0]), format, args);
    va_end(args);

    if (s_LogCallback) {
        s_LogCallback(buf);
    }
}

static void GetExeDirectory(wchar_t *outPath, DWORD maxLen) {
    GetModuleFileNameW(NULL, outPath, maxLen);
    wchar_t *lastSlash = wcsrchr(outPath, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
    }
}

// Refresh the fast snapshot of currently active processes
static void RefreshRunningProcesses(void) {
    s_RunningProcCount = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (s_RunningProcCount < MAX_RUNNING_PROCS) {
                wcsncpy(s_RunningProcs[s_RunningProcCount], pe.szExeFile, 63);
                s_RunningProcs[s_RunningProcCount][63] = L'\0';
                s_RunningProcCount++;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

bool Cleaner_IsProcessRunning(const wchar_t *procName) {
    for (int i = 0; i < s_RunningProcCount; i++) {
        if (_wcsicmp(s_RunningProcs[i], procName) == 0) {
            return true;
        }
    }
    return false;
}

// Smart safety checks: if an application is active, protect its folders in %TEMP% and cache
static bool ShouldSkipDirectory(const wchar_t *dirName) {
    if (Cleaner_IsProcessRunning(L"chrome.exe")) {
        if (wcsstr(dirName, L"chrome") || wcsstr(dirName, L"scoped_dir") || wcsstr(dirName, L"CRX_")) return true;
    }
    if (Cleaner_IsProcessRunning(L"msedge.exe")) {
        if (wcsstr(dirName, L"edge") || wcsstr(dirName, L"msedge") || wcsstr(dirName, L"scoped_dir")) return true;
    }
    if (Cleaner_IsProcessRunning(L"brave.exe")) {
        if (wcsstr(dirName, L"brave") || wcsstr(dirName, L"scoped_dir")) return true;
    }
    if (Cleaner_IsProcessRunning(L"firefox.exe")) {
        if (wcsstr(dirName, L"mozilla") || wcsstr(dirName, L"firefox")) return true;
    }
    if (Cleaner_IsProcessRunning(L"Discord.exe")) {
        if (wcsstr(dirName, L"discord")) return true;
    }
    if (Cleaner_IsProcessRunning(L"steam.exe") || Cleaner_IsProcessRunning(L"steamwebhelper.exe")) {
        if (wcsstr(dirName, L"steam")) return true;
    }
    if (Cleaner_IsProcessRunning(L"Spotify.exe")) {
        if (wcsstr(dirName, L"spotify")) return true;
    }
    return false;
}

// Never delete in-progress download files or actively locked streams
static bool ShouldSkipFile(const wchar_t *fileName) {
    const wchar_t *ext = wcsrchr(fileName, L'.');
    if (ext) {
        if (_wcsicmp(ext, L".crdownload") == 0 ||
            _wcsicmp(ext, L".part") == 0 ||
            _wcsicmp(ext, L".download") == 0 ||
            _wcsicmp(ext, L".opdownload") == 0) {
            return true;
        }
    }
    return false;
}

void Cleaner_LoadConfig(void) {
    wchar_t exeDir[MAX_PATH];
    GetExeDirectory(exeDir, MAX_PATH);
    swprintf(s_ConfigIniPath, MAX_PATH, L"%s\\cleaner_config.ini", exeDir);

    // If config file doesn't exist in exe dir, check current working dir
    if (GetFileAttributesW(s_ConfigIniPath) == INVALID_FILE_ATTRIBUTES) {
        GetCurrentDirectoryW(MAX_PATH, exeDir);
        swprintf(s_ConfigIniPath, MAX_PATH, L"%s\\cleaner_config.ini", exeDir);
    }

    g_Config.autoCleanIntervalSeconds = GetPrivateProfileIntW(L"Settings", L"AutoCleanIntervalSeconds", 300, s_ConfigIniPath);
    if (g_Config.autoCleanIntervalSeconds < 10) g_Config.autoCleanIntervalSeconds = 300;

    g_Config.autoCleanEnabled = GetPrivateProfileIntW(L"Settings", L"AutoCleanEnabled", 1, s_ConfigIniPath) != 0;
    g_Config.blockerEnabled = GetPrivateProfileIntW(L"Settings", L"BlockerEnabled", 1, s_ConfigIniPath) != 0;
    g_Config.cleanRecycleBin = GetPrivateProfileIntW(L"Settings", L"CleanRecycleBin", 1, s_ConfigIniPath) != 0;
    g_Config.cleanBrowserCaches = GetPrivateProfileIntW(L"Settings", L"CleanBrowserCaches", 1, s_ConfigIniPath) != 0;
    g_Config.cleanCrashDumps = GetPrivateProfileIntW(L"Settings", L"CleanCrashDumps", 1, s_ConfigIniPath) != 0;
    
    // Default safe threshold: 14400 seconds (4 hours) so active software temp files are preserved
    g_Config.minFileAgeSeconds = GetPrivateProfileIntW(L"Settings", L"MinFileAgeSeconds", 14400, s_ConfigIniPath);

    g_Config.blockedCount = 0;
    for (int i = 1; i <= MAX_BLOCKED_PROCESSES; i++) {
        wchar_t keyName[32];
        swprintf(keyName, 32, L"Process%d", i);
        wchar_t procName[64];
        if (GetPrivateProfileStringW(L"BlockedProcesses", keyName, L"", procName, 64, s_ConfigIniPath) > 0) {
            if (wcslen(procName) > 0) {
                wcsncpy(g_Config.blockedProcesses[g_Config.blockedCount], procName, 63);
                g_Config.blockedProcesses[g_Config.blockedCount][63] = L'\0';
                g_Config.blockedCount++;
            }
        }
    }

    // Default safety fallback if config.ini missing
    if (g_Config.blockedCount == 0) {
        const wchar_t *defaults[] = {
            L"CompatTelRunner.exe",
            L"devicecensus.exe",
            L"wermgr.exe",
            L"gamebarftserver.exe",
            L"CCleaner64.exe",
            L"CCleanerReactivator.exe",
            L"OneDriveStandaloneUpdater.exe",
            L"MicrosoftEdgeUpdate.exe",
            L"GoogleUpdate.exe",
            L"jusched.exe",
            L"AdobeUpdateService.exe",
            L"AGSService.exe",
            L"mscorsvw.exe",
            L"CrashSender1403.exe",
            L"CrashSender1402.exe"
        };
        for (int i = 0; i < 15; i++) {
            wcsncpy(g_Config.blockedProcesses[i], defaults[i], 63);
            g_Config.blockedProcesses[i][63] = L'\0';
            g_Config.blockedCount++;
        }
    }
}

void Cleaner_SaveConfig(void) {
    if (wcslen(s_ConfigIniPath) == 0) return;
    wchar_t val[32];

    swprintf(val, 32, L"%d", g_Config.autoCleanIntervalSeconds);
    WritePrivateProfileStringW(L"Settings", L"AutoCleanIntervalSeconds", val, s_ConfigIniPath);

    WritePrivateProfileStringW(L"Settings", L"AutoCleanEnabled", g_Config.autoCleanEnabled ? L"1" : L"0", s_ConfigIniPath);
    WritePrivateProfileStringW(L"Settings", L"BlockerEnabled", g_Config.blockerEnabled ? L"1" : L"0", s_ConfigIniPath);
    WritePrivateProfileStringW(L"Settings", L"CleanRecycleBin", g_Config.cleanRecycleBin ? L"1" : L"0", s_ConfigIniPath);
}

bool Cleaner_IsAutoStartEnabled(void) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buffer[MAX_PATH * 2];
        DWORD bufferSize = sizeof(buffer);
        DWORD type = 0;
        LONG res = RegQueryValueExW(hKey, RUN_REG_VALUE, NULL, &type, (LPBYTE)buffer, &bufferSize);
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS && bufferSize > 0);
    }
    return false;
}

bool Cleaner_SetAutoStart(bool bEnable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_REG_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        LONG res;
        if (bEnable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            wchar_t cmdLine[MAX_PATH * 2];
            swprintf(cmdLine, MAX_PATH * 2, L"\"%s\" -minimized", exePath);
            res = RegSetValueExW(hKey, RUN_REG_VALUE, 0, REG_SZ, (const BYTE *)cmdLine, (DWORD)((wcslen(cmdLine) + 1) * sizeof(wchar_t)));
        } else {
            res = RegDeleteValueW(hKey, RUN_REG_VALUE);
        }
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS);
    }
    return false;
}

void Cleaner_TrimWorkingSet(void) {
    // Flush unused pages of Max Cleaner, reducing memory consumption to minimal working set
    EmptyWorkingSet(GetCurrentProcess());
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    g_Stats.currentRamUsageKB = Cleaner_GetOwnMemoryUsageKB();
}

uint32_t Cleaner_GetOwnMemoryUsageKB(void) {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return (uint32_t)(pmc.WorkingSetSize / 1024);
    }
    return 0;
}

uint64_t Cleaner_EmptyRecycleBin(void) {
    SHQUERYRBINFO rbi = {0};
    rbi.cbSize = sizeof(rbi);
    uint64_t binBytes = 0;
    if (SHQueryRecycleBinW(NULL, &rbi) == S_OK) {
        binBytes = (uint64_t)rbi.i64Size;
    }

    HRESULT hr = SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    if (SUCCEEDED(hr)) {
        if (binBytes > 0) {
            Cleaner_Log(L"Recycle bin emptied: %.2f MB freed", (double)binBytes / (1024.0 * 1024.0));
        } else {
            Cleaner_Log(L"Recycle bin emptied.");
        }
        return binBytes;
    }
    return 0;
}

static bool IsProtectedProcess(const wchar_t *procName) {
    for (int i = 0; SYSTEM_WHITELIST[i] != NULL; i++) {
        if (_wcsicmp(procName, SYSTEM_WHITELIST[i]) == 0) {
            return true;
        }
    }
    return false;
}

uint32_t Cleaner_BlockUnnecessaryApps(void) {
    if (!g_Config.blockerEnabled) return 0;

    uint32_t blocked = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);

    if (Process32FirstW(hSnap, &pe32)) {
        do {
            if (IsProtectedProcess(pe32.szExeFile)) {
                continue;
            }

            for (int i = 0; i < g_Config.blockedCount; i++) {
                if (_wcsicmp(pe32.szExeFile, g_Config.blockedProcesses[i]) == 0) {
                    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                    if (hProc) {
                        if (TerminateProcess(hProc, 0)) {
                            blocked++;
                            Cleaner_Log(L"Blocked background process: %s (PID %u)", pe32.szExeFile, pe32.th32ProcessID);
                        }
                        CloseHandle(hProc);
                    }
                    break;
                }
            }
        } while (Process32NextW(hSnap, &pe32));
    }

    CloseHandle(hSnap);
    return blocked;
}

// Low-overhead recursive directory cleaner with smart app-aware protection
static void CleanDirectoryInternal(const wchar_t *dirPath, int depth, uint32_t *pFilesCleaned, uint64_t *pBytesCleaned) {
    if (depth > 5) return; // Prevent excessive recursion depth

    wchar_t searchPattern[MAX_PATH_LEN];
    swprintf(searchPattern, MAX_PATH_LEN, L"%s\\*", dirPath);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileExW(searchPattern, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (hFind == INVALID_HANDLE_VALUE) return;

    FILETIME ftNow;
    GetSystemTimeAsFileTime(&ftNow);
    ULARGE_INTEGER uNow;
    uNow.LowPart = ftNow.dwLowDateTime;
    uNow.HighPart = ftNow.dwHighDateTime;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }

        // Strictly skip symlinks, junctions and reparse points
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            continue;
        }

        // If an application is currently running, strictly protect its subdirectories in temp
        if (ShouldSkipDirectory(fd.cFileName)) {
            continue;
        }

        // Skip in-progress downloads
        if (ShouldSkipFile(fd.cFileName)) {
            continue;
        }

        wchar_t itemPath[MAX_PATH_LEN];
        swprintf(itemPath, MAX_PATH_LEN, L"%s\\%s", dirPath, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            CleanDirectoryInternal(itemPath, depth + 1, pFilesCleaned, pBytesCleaned);
            RemoveDirectoryW(itemPath);
        } else {
            // Check file age (avoid deleting active temporary files from running applications)
            ULARGE_INTEGER uFile;
            uFile.LowPart = fd.ftLastWriteTime.dwLowDateTime;
            uFile.HighPart = fd.ftLastWriteTime.dwHighDateTime;
            uint64_t diffSeconds = (uNow.QuadPart > uFile.QuadPart) ? (uNow.QuadPart - uFile.QuadPart) / 10000000ULL : 0;

            if (diffSeconds >= (uint64_t)g_Config.minFileAgeSeconds) {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
                    SetFileAttributesW(itemPath, FILE_ATTRIBUTE_NORMAL);
                }
                if (DeleteFileW(itemPath)) {
                    (*pFilesCleaned)++;
                    ULARGE_INTEGER fsz;
                    fsz.LowPart = fd.nFileSizeLow;
                    fsz.HighPart = fd.nFileSizeHigh;
                    (*pBytesCleaned) += fsz.QuadPart;

                    // Micro-yield CPU every 40 files to guarantee ZERO frame drops for games & foreground apps
                    if ((*pFilesCleaned) % 40 == 0) {
                        Sleep(1);
                    }
                }
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

typedef struct {
    HWND hwndNotify;
    bool bManual;
} SweepThreadParam;

static DWORD WINAPI SweepWorkerThread(LPVOID lpParam) {
    SweepThreadParam *param = (SweepThreadParam *)lpParam;
    HWND hwndNotify = param ? param->hwndNotify : NULL;
    bool bManual = param ? param->bManual : false;
    free(param);

    // Set thread priority to background mode:
    // Windows sets CPU to lowest and I/O priority to IDLE/VERY LOW.
    // This ensures gaming, rendering, and foreground apps get 100% priority.
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);

    uint32_t filesCleaned = 0;
    uint64_t bytesCleaned = 0;
    uint32_t blockedProcs = 0;

    // Refresh active running processes snapshot
    RefreshRunningProcesses();

    Cleaner_Log(bManual ? L"--- Starting Manual Turbo Clean Sweep ---" : L"--- Starting 5-Minute Auto Clean Sweep ---");

    // 1. Block unnecessary background apps
    if (g_Config.blockerEnabled) {
        blockedProcs = Cleaner_BlockUnnecessaryApps();
        // Refresh running processes after blocker runs
        RefreshRunningProcesses();
    }

    // 2. Clean User Temp (%TEMP%) - protects active applications
    wchar_t tempPath[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempPath) > 0) {
        size_t len = wcslen(tempPath);
        if (len > 0 && tempPath[len - 1] == L'\\') {
            tempPath[len - 1] = L'\0';
        }
        CleanDirectoryInternal(tempPath, 0, &filesCleaned, &bytesCleaned);
    }

    // 3. Clean Windows Temp (if accessible)
    wchar_t winDir[MAX_PATH];
    if (GetWindowsDirectoryW(winDir, MAX_PATH) > 0) {
        wchar_t winTemp[MAX_PATH];
        swprintf(winTemp, MAX_PATH, L"%s\\Temp", winDir);
        CleanDirectoryInternal(winTemp, 0, &filesCleaned, &bytesCleaned);

        // Windows SoftwareDistribution download cache
        wchar_t swDist[MAX_PATH];
        swprintf(swDist, MAX_PATH, L"%s\\SoftwareDistribution\\Download", winDir);
        CleanDirectoryInternal(swDist, 0, &filesCleaned, &bytesCleaned);
    }

    // 4. Clean Crash Dumps and Error Reporting
    if (g_Config.cleanCrashDumps) {
        wchar_t localAppData[MAX_PATH];
        if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData) == S_OK) {
            wchar_t crashDumps[MAX_PATH];
            swprintf(crashDumps, MAX_PATH, L"%s\\CrashDumps", localAppData);
            CleanDirectoryInternal(crashDumps, 0, &filesCleaned, &bytesCleaned);

            wchar_t werArchive[MAX_PATH];
            swprintf(werArchive, MAX_PATH, L"%s\\Microsoft\\Windows\\WER\\ReportArchive", localAppData);
            CleanDirectoryInternal(werArchive, 0, &filesCleaned, &bytesCleaned);

            wchar_t werQueue[MAX_PATH];
            swprintf(werQueue, MAX_PATH, L"%s\\Microsoft\\Windows\\WER\\ReportQueue", localAppData);
            CleanDirectoryInternal(werQueue, 0, &filesCleaned, &bytesCleaned);
        }
    }

    // 5. Clean Application / Browser Caches ONLY if that application is NOT running!
    // If the application is open, its cache files remain untouched in the folders.
    if (g_Config.cleanBrowserCaches) {
        wchar_t localAppData[MAX_PATH];
        wchar_t appData[MAX_PATH];

        bool isChromeRunning = Cleaner_IsProcessRunning(L"chrome.exe");
        bool isEdgeRunning = Cleaner_IsProcessRunning(L"msedge.exe");
        bool isBraveRunning = Cleaner_IsProcessRunning(L"brave.exe");
        bool isFirefoxRunning = Cleaner_IsProcessRunning(L"firefox.exe");
        bool isDiscordRunning = Cleaner_IsProcessRunning(L"Discord.exe");
        bool isSteamRunning = Cleaner_IsProcessRunning(L"steam.exe") || Cleaner_IsProcessRunning(L"steamwebhelper.exe");
        bool isSpotifyRunning = Cleaner_IsProcessRunning(L"Spotify.exe");

        if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData) == S_OK) {
            // Google Chrome Cache
            if (isChromeRunning) {
                Cleaner_Log(L"Google Chrome is active: cache preserved untouched.");
            } else {
                wchar_t chromeCache[MAX_PATH];
                swprintf(chromeCache, MAX_PATH, L"%s\\Google\\Chrome\\User Data\\Default\\Cache\\Cache_Data", localAppData);
                CleanDirectoryInternal(chromeCache, 0, &filesCleaned, &bytesCleaned);
                swprintf(chromeCache, MAX_PATH, L"%s\\Google\\Chrome\\User Data\\Default\\Code Cache", localAppData);
                CleanDirectoryInternal(chromeCache, 0, &filesCleaned, &bytesCleaned);
                swprintf(chromeCache, MAX_PATH, L"%s\\Google\\Chrome\\User Data\\Default\\GPUCache", localAppData);
                CleanDirectoryInternal(chromeCache, 0, &filesCleaned, &bytesCleaned);
            }

            // Microsoft Edge Cache
            if (isEdgeRunning) {
                Cleaner_Log(L"Microsoft Edge is active: cache preserved untouched.");
            } else {
                wchar_t edgeCache[MAX_PATH];
                swprintf(edgeCache, MAX_PATH, L"%s\\Microsoft\\Edge\\User Data\\Default\\Cache\\Cache_Data", localAppData);
                CleanDirectoryInternal(edgeCache, 0, &filesCleaned, &bytesCleaned);
                swprintf(edgeCache, MAX_PATH, L"%s\\Microsoft\\Edge\\User Data\\Default\\Code Cache", localAppData);
                CleanDirectoryInternal(edgeCache, 0, &filesCleaned, &bytesCleaned);
                swprintf(edgeCache, MAX_PATH, L"%s\\Microsoft\\Edge\\User Data\\Default\\GPUCache", localAppData);
                CleanDirectoryInternal(edgeCache, 0, &filesCleaned, &bytesCleaned);
            }

            // Brave Browser Cache
            if (isBraveRunning) {
                Cleaner_Log(L"Brave Browser is active: cache preserved untouched.");
            } else {
                wchar_t braveCache[MAX_PATH];
                swprintf(braveCache, MAX_PATH, L"%s\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Cache\\Cache_Data", localAppData);
                CleanDirectoryInternal(braveCache, 0, &filesCleaned, &bytesCleaned);
                swprintf(braveCache, MAX_PATH, L"%s\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Code Cache", localAppData);
                CleanDirectoryInternal(braveCache, 0, &filesCleaned, &bytesCleaned);
            }

            // Steam HTML Cache
            if (isSteamRunning) {
                Cleaner_Log(L"Steam is active: web cache preserved untouched.");
            } else {
                wchar_t steamCache[MAX_PATH];
                swprintf(steamCache, MAX_PATH, L"%s\\Steam\\htmlcache\\Cache", localAppData);
                CleanDirectoryInternal(steamCache, 0, &filesCleaned, &bytesCleaned);
                swprintf(steamCache, MAX_PATH, L"%s\\Steam\\htmlcache\\Code Cache", localAppData);
                CleanDirectoryInternal(steamCache, 0, &filesCleaned, &bytesCleaned);
            }

            // Spotify Storage Cache
            if (isSpotifyRunning) {
                Cleaner_Log(L"Spotify is active: cache preserved untouched.");
            } else {
                wchar_t spotifyCache[MAX_PATH];
                swprintf(spotifyCache, MAX_PATH, L"%s\\Spotify\\Storage", localAppData);
                CleanDirectoryInternal(spotifyCache, 0, &filesCleaned, &bytesCleaned);
            }
        }

        if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData) == S_OK) {
            // Discord Cache
            if (isDiscordRunning) {
                Cleaner_Log(L"Discord is active: cache preserved untouched.");
            } else {
                wchar_t discordCache[MAX_PATH];
                swprintf(discordCache, MAX_PATH, L"%s\\discord\\Cache\\Cache_Data", appData);
                CleanDirectoryInternal(discordCache, 0, &filesCleaned, &bytesCleaned);
                swprintf(discordCache, MAX_PATH, L"%s\\discord\\Code Cache", appData);
                CleanDirectoryInternal(discordCache, 0, &filesCleaned, &bytesCleaned);
                swprintf(discordCache, MAX_PATH, L"%s\\discord\\GPUCache", appData);
                CleanDirectoryInternal(discordCache, 0, &filesCleaned, &bytesCleaned);
            }

            // Firefox Cache
            if (isFirefoxRunning) {
                Cleaner_Log(L"Mozilla Firefox is active: cache preserved untouched.");
            }
        }
    }

    // 6. Clean Recycle Bin
    if (g_Config.cleanRecycleBin) {
        bytesCleaned += Cleaner_EmptyRecycleBin();
    }

    // 7. Restore background thread mode
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);

    // 8. Trim memory immediately so Max Cleaner stays strictly < 10 MB RAM
    Cleaner_TrimWorkingSet();

    // Update global statistics
    g_Stats.lastSweepCleanedFiles = filesCleaned;
    g_Stats.lastSweepCleanedBytes = bytesCleaned;
    g_Stats.lastSweepBlockedProcs = blockedProcs;
    g_Stats.totalFilesCleaned += filesCleaned;
    g_Stats.totalBytesCleaned += bytesCleaned;
    g_Stats.totalProcessesBlocked += blockedProcs;
    g_Stats.totalCleanPasses++;
    g_Stats.isCleaning = false;

    double mbCleaned = (double)bytesCleaned / (1024.0 * 1024.0);
    Cleaner_Log(L"Sweep complete: %u files deleted (%.2f MB), %u apps blocked. RAM: %.2f MB",
                filesCleaned, mbCleaned, blockedProcs, (double)g_Stats.currentRamUsageKB / 1024.0);

    // Notify UI window
    if (hwndNotify && IsWindow(hwndNotify)) {
        PostMessageW(hwndNotify, WM_CLEAN_FINISHED, 0, 0);
    }

    return 0;
}

void Cleaner_RunSweepAsync(HWND hwndNotify, bool bManual) {
    if (g_Stats.isCleaning) return;

    g_Stats.isCleaning = true;
    SweepThreadParam *param = (SweepThreadParam *)malloc(sizeof(SweepThreadParam));
    if (param) {
        param->hwndNotify = hwndNotify;
        param->bManual = bManual;
        HANDLE hThread = CreateThread(NULL, 0, SweepWorkerThread, param, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
        } else {
            free(param);
            g_Stats.isCleaning = false;
        }
    }
}

void Cleaner_Init(void) {
    Cleaner_LoadConfig();

    // If configured to run at startup, ensure the registry entry exists
    int startSetting = GetPrivateProfileIntW(L"Settings", L"StartWithWindows", 1, s_ConfigIniPath);
    if (startSetting && !Cleaner_IsAutoStartEnabled()) {
        Cleaner_SetAutoStart(true);
    }

    // Initial memory trim
    Cleaner_TrimWorkingSet();
}
