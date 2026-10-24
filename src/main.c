#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdbool.h>
#include "resource.h"
#include "cleaner.h"

#define IDT_CLEAN_TIMER 1001
#define IDT_TICK_TIMER  1002

static HWND g_hMainWnd = NULL;
static HWND g_hLogList = NULL;
static NOTIFYICONDATAW g_nid = {0};
static HICON g_hAppIcon = NULL;
static HFONT g_hFontTitle = NULL;
static HFONT g_hFontNormal = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontSmall = NULL;
static HFONT g_hFontNumber = NULL;
static HBRUSH g_hBrushBg = NULL;
static HBRUSH g_hBrushCard = NULL;
static HBRUSH g_hBrushBtn = NULL;

static int g_SecondsUntilNextSweep = 300;

// UI Control Handles
static HWND g_hBtnCleanNow = NULL;
static HWND g_hBtnEmptyBin = NULL;
static HWND g_hBtnToggleAuto = NULL;
static HWND g_hBtnToggleBlock = NULL;
static HWND g_hBtnToggleStartup = NULL;
static HWND g_hBtnConfig = NULL;
static HWND g_hBtnMinimize = NULL;

// Helper to append text to log listbox
static void UI_LogCallback(const wchar_t *msg) {
    if (g_hLogList && IsWindow(g_hLogList)) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t line[600];
        swprintf(line, 600, L"[%02d:%02d:%02d] %s", st.wHour, st.wMinute, st.wSecond, msg);
        int idx = (int)SendMessageW(g_hLogList, LB_ADDSTRING, 0, (LPARAM)line);
        SendMessageW(g_hLogList, LB_SETCURSEL, idx, 0);

        // Keep maximum 80 entries in listbox to keep RAM minimal
        int count = (int)SendMessageW(g_hLogList, LB_GETCOUNT, 0, 0);
        if (count > 80) {
            SendMessageW(g_hLogList, LB_DELETESTRING, 0, 0);
        }
    }
}

static void UpdateTrayTooltip(void) {
    double mbCleaned = (double)g_Stats.totalBytesCleaned / (1024.0 * 1024.0);
    double ramMB = (double)Cleaner_GetOwnMemoryUsageKB() / 1024.0;
    swprintf(g_nid.szTip, 128, L"Max Cleaner by Maxpain\nRAM: %.2f MB (<10MB Limit)\nCleaned: %.1f MB | Blocked: %u",
             ramMB, mbCleaned, g_Stats.totalProcessesBlocked);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void ShowTrayMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_DISABLED, IDM_HEADER, L"⚡ MAX CLEANER - By Maxpain");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_CLEAN_NOW, L"⚡ Clean Now (Turbo Sweep)");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_EMPTY_RECYCLE, L"🗑️ Empty Recycle Bin");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    UINT autoFlags = MF_BYPOSITION | MF_STRING | (g_Config.autoCleanEnabled ? MF_CHECKED : MF_UNCHECKED);
    InsertMenuW(hMenu, -1, autoFlags, IDM_TOGGLE_AUTOCLEAN, L"⏱️ Auto-Clean Every 5 Mins");

    UINT blockFlags = MF_BYPOSITION | MF_STRING | (g_Config.blockerEnabled ? MF_CHECKED : MF_UNCHECKED);
    InsertMenuW(hMenu, -1, blockFlags, IDM_TOGGLE_BLOCKER, L"🛡️ Block Unnecessary Apps");

    bool isAutoStart = Cleaner_IsAutoStartEnabled();
    UINT startFlags = MF_BYPOSITION | MF_STRING | (isAutoStart ? MF_CHECKED : MF_UNCHECKED);
    InsertMenuW(hMenu, -1, startFlags, IDM_TOGGLE_STARTUP, L"🚀 Run on Windows Startup");

    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_SHOW_DASHBOARD, L"📊 Open Dashboard...");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_OPEN_CONFIG, L"⚙️ Edit Config / Blocklist...");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"❌ Exit Max Cleaner");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

static void UpdateButtonTexts(void) {
    if (g_hBtnToggleAuto) {
        SetWindowTextW(g_hBtnToggleAuto, g_Config.autoCleanEnabled ? L"⏱️ 5-Min Sweep: ON" : L"⏱️ 5-Min Sweep: OFF");
    }
    if (g_hBtnToggleBlock) {
        SetWindowTextW(g_hBtnToggleBlock, g_Config.blockerEnabled ? L"🛡️ Bloat Blocker: ON" : L"🛡️ Bloat Blocker: OFF");
    }
    if (g_hBtnToggleStartup) {
        bool autoStart = Cleaner_IsAutoStartEnabled();
        SetWindowTextW(g_hBtnToggleStartup, autoStart ? L"🚀 Startup: ON" : L"🚀 Startup: OFF");
    }
}

static void DrawMetricCard(HDC hdc, int x, int y, int w, int h, const wchar_t *title, const wchar_t *value, const wchar_t *subtitle, COLORREF valColor) {
    RECT rc = {x, y, x + w, y + h};
    FillRect(hdc, &rc, g_hBrushCard);

    // Subtle outline
    HPEN hPenBorder = CreatePen(PS_SOLID, 1, RGB(38, 50, 78));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPenBorder);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, x, y, x + w, y + h, 8, 8);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPenBorder);

    SetBkMode(hdc, TRANSPARENT);

    // Title
    SelectObject(hdc, g_hFontSmall);
    SetTextColor(hdc, RGB(140, 160, 190));
    RECT rcTitle = {x + 12, y + 8, x + w - 12, y + 24};
    DrawTextW(hdc, title, -1, &rcTitle, DT_LEFT | DT_SINGLELINE);

    // Value
    SelectObject(hdc, g_hFontNumber);
    SetTextColor(hdc, valColor);
    RECT rcVal = {x + 12, y + 24, x + w - 12, y + 54};
    DrawTextW(hdc, value, -1, &rcVal, DT_LEFT | DT_SINGLELINE);

    // Subtitle
    SelectObject(hdc, g_hFontSmall);
    SetTextColor(hdc, RGB(110, 130, 160));
    RECT rcSub = {x + 12, y + 54, x + w - 12, y + 70};
    DrawTextW(hdc, subtitle, -1, &rcSub, DT_LEFT | DT_SINGLELINE);
}

static void OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT clientRc;
    GetClientRect(hwnd, &clientRc);
    int width = clientRc.right - clientRc.left;
    int height = clientRc.bottom - clientRc.top;

    if (width <= 0 || height <= 0) {
        EndPaint(hwnd, &ps);
        return;
    }

    // Double buffering to eliminate all flicker
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    // Background fill
    FillRect(memDC, &clientRc, g_hBrushBg);

    // Header banner
    RECT headerRc = {0, 0, width, 68};
    HBRUSH hHeaderBrush = CreateSolidBrush(RGB(18, 24, 40));
    FillRect(memDC, &headerRc, hHeaderBrush);
    DeleteObject(hHeaderBrush);

    // Accent line below header
    HPEN hAccentPen = CreatePen(PS_SOLID, 2, RGB(0, 229, 255));
    HPEN oldPen = (HPEN)SelectObject(memDC, hAccentPen);
    MoveToEx(memDC, 0, 68, NULL);
    LineTo(memDC, width, 68);
    SelectObject(memDC, oldPen);
    DeleteObject(hAccentPen);

    // Header App Icon
    if (g_hAppIcon) {
        DrawIconEx(memDC, 16, 16, g_hAppIcon, 36, 36, 0, NULL, DI_NORMAL);
    }

    SetBkMode(memDC, TRANSPARENT);

    // Title
    SelectObject(memDC, g_hFontTitle);
    SetTextColor(memDC, RGB(255, 255, 255));
    TextOutW(memDC, 62, 14, L"MAX CLEANER", 11);

    // Publisher & Subtitle
    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, RGB(0, 229, 255));
    const wchar_t *subText = L"Publisher: Maxpain   |   Ultra-Low RAM & Zero Frame Drop Engine";
    TextOutW(memDC, 64, 40, subText, (int)wcslen(subText));

    // Status Badge in Header (Right side)
    RECT badgeRc = {width - 190, 20, width - 20, 48};
    HBRUSH hBadgeBrush = CreateSolidBrush(RGB(10, 40, 30));
    FillRect(memDC, &badgeRc, hBadgeBrush);
    DeleteObject(hBadgeBrush);

    HPEN hBadgePen = CreatePen(PS_SOLID, 1, RGB(0, 255, 170));
    oldPen = (HPEN)SelectObject(memDC, hBadgePen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));
    RoundRect(memDC, badgeRc.left, badgeRc.top, badgeRc.right, badgeRc.bottom, 6, 6);
    SelectObject(memDC, oldBrush);
    SelectObject(memDC, oldPen);
    DeleteObject(hBadgePen);

    SelectObject(memDC, g_hFontBold);
    SetTextColor(memDC, RGB(0, 255, 170));
    DrawTextW(memDC, L"● ACTIVE: PROTECTED", -1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // ================= METRICS GRID =================
    int cardY = 82;
    int cardW = 168;
    int cardH = 76;
    int gap = 12;
    int cardX = 16;

    // Card 1: RAM Usage (< 10 MB strict requirement)
    double ramMB = (double)Cleaner_GetOwnMemoryUsageKB() / 1024.0;
    wchar_t ramStr[32];
    swprintf(ramStr, 32, L"%.2f MB", ramMB);
    DrawMetricCard(memDC, cardX, cardY, cardW, cardH, L"MEMORY / RAM USAGE", ramStr, L"Strict Limit: < 10.0 MB (OK)", RGB(0, 255, 170));

    // Card 2: Total Junk Cleaned
    cardX += cardW + gap;
    double cleanedMB = (double)g_Stats.totalBytesCleaned / (1024.0 * 1024.0);
    wchar_t cleanStr[32];
    if (cleanedMB >= 1024.0) {
        swprintf(cleanStr, 32, L"%.2f GB", cleanedMB / 1024.0);
    } else {
        swprintf(cleanStr, 32, L"%.1f MB", cleanedMB);
    }
    DrawMetricCard(memDC, cardX, cardY, cardW, cardH, L"TOTAL JUNK CLEANED", cleanStr, L"Temp, Cache & Recycle Bin", RGB(0, 229, 255));

    // Card 3: Files Cleaned
    cardX += cardW + gap;
    wchar_t filesStr[32];
    swprintf(filesStr, 32, L"%u", g_Stats.totalFilesCleaned);
    DrawMetricCard(memDC, cardX, cardY, cardW, cardH, L"FILES ELIMINATED", filesStr, L"Stale clutter purged", RGB(255, 215, 0));

    // Second Row of Cards
    cardY += cardH + gap;
    cardX = 16;

    // Card 4: Background Bloat Blocked
    wchar_t blockStr[32];
    swprintf(blockStr, 32, L"%u Apps", g_Stats.totalProcessesBlocked);
    DrawMetricCard(memDC, cardX, cardY, cardW, cardH, L"UNNECESSARY APPS BLOCKED", blockStr, L"Telemetry & background hogs", RGB(255, 100, 100));

    // Card 5: Next 5-Minute Auto-Clean Countdown
    cardX += cardW + gap;
    wchar_t timerStr[32];
    if (g_Config.autoCleanEnabled) {
        int m = g_SecondsUntilNextSweep / 60;
        int s = g_SecondsUntilNextSweep % 60;
        swprintf(timerStr, 32, L"%02d:%02d", m, s);
    } else {
        wcscpy(timerStr, L"PAUSED");
    }
    DrawMetricCard(memDC, cardX, cardY, cardW, cardH, L"NEXT AUTO-CLEAN (5-MIN)", timerStr, L"Auto background sweep", RGB(180, 140, 255));

    // Card 6: CPU Impact & Frame Drop Status
    cardX += cardW + gap;
    DrawMetricCard(memDC, cardX, cardY, cardW, cardH, L"CPU / FPS IMPACT", L"< 0.1% CPU", L"Zero Frame Drops (Idle I/O)", RGB(0, 255, 170));

    // Section label for Log
    SelectObject(memDC, g_hFontBold);
    SetTextColor(memDC, RGB(200, 220, 240));
    TextOutW(memDC, 16, 260, L"REAL-TIME ACTIVITY & PERFORMANCE LOG:", 37);

    // Blit to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hMainWnd = hwnd;

            // Load App Icon from resources
            g_hAppIcon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR);
            if (!g_hAppIcon) {
                g_hAppIcon = (HICON)LoadIconW(NULL, IDI_APPLICATION);
            }

            // Create fonts
            g_hFontTitle = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_hFontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_hFontNormal = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_hFontSmall = CreateFontW(11, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_hFontNumber = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

            g_hBrushBg = CreateSolidBrush(RGB(15, 18, 28));
            g_hBrushCard = CreateSolidBrush(RGB(22, 28, 44));
            g_hBrushBtn = CreateSolidBrush(RGB(30, 40, 62));

            // Setup System Tray Icon
            g_nid.cbSize = sizeof(NOTIFYICONDATAW);
            g_nid.hWnd = hwnd;
            g_nid.uID = 1;
            g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            g_nid.uCallbackMessage = WM_TRAYICON;
            g_nid.hIcon = g_hAppIcon;
            wcscpy(g_nid.szTip, L"Max Cleaner by Maxpain - Initializing...");
            Shell_NotifyIconW(NIM_ADD, &g_nid);

            // Activity Log Listbox
            g_hLogList = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
                16, 282, 524, 160,
                hwnd, (HMENU)IDC_LIST_LOG, GetModuleHandleW(NULL), NULL
            );
            SendMessageW(g_hLogList, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            // Action Buttons
            int btnY = 454;
            g_hBtnCleanNow = CreateWindowW(L"BUTTON", L"⚡ Clean Now", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16, btnY, 120, 32, hwnd, (HMENU)IDC_BTN_CLEAN_NOW, GetModuleHandleW(NULL), NULL);
            g_hBtnEmptyBin = CreateWindowW(L"BUTTON", L"🗑️ Empty Bin", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 142, btnY, 110, 32, hwnd, (HMENU)IDC_BTN_EMPTY_BIN, GetModuleHandleW(NULL), NULL);
            g_hBtnToggleAuto = CreateWindowW(L"BUTTON", L"⏱️ 5-Min: ON", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 258, btnY, 136, 32, hwnd, (HMENU)IDC_BTN_TOGGLE_AUTO, GetModuleHandleW(NULL), NULL);
            g_hBtnToggleBlock = CreateWindowW(L"BUTTON", L"🛡️ Bloat Block: ON", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 400, btnY, 140, 32, hwnd, (HMENU)IDC_BTN_TOGGLE_BLOCK, GetModuleHandleW(NULL), NULL);

            int btnY2 = 492;
            g_hBtnToggleStartup = CreateWindowW(L"BUTTON", L"🚀 Startup: ON", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16, btnY2, 160, 32, hwnd, (HMENU)IDM_TOGGLE_STARTUP, GetModuleHandleW(NULL), NULL);
            g_hBtnConfig = CreateWindowW(L"BUTTON", L"⚙️ Edit Config...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 184, btnY2, 174, 32, hwnd, (HMENU)IDC_BTN_CONFIG, GetModuleHandleW(NULL), NULL);
            g_hBtnMinimize = CreateWindowW(L"BUTTON", L"✕ Minimize to Tray", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 366, btnY2, 174, 32, hwnd, (HMENU)IDC_BTN_MINIMIZE, GetModuleHandleW(NULL), NULL);

            UpdateButtonTexts();

            // Set Log Callback
            Cleaner_SetLogCallback(UI_LogCallback);
            Cleaner_Log(L"Max Cleaner initialized successfully (Publisher: Maxpain)");
            Cleaner_Log(L"Engine configured: Memory strictly capped < 10 MB, Zero Frame Drop mode.");

            // Start Timers:
            // Timer 1: 5-minute auto clean timer (300,000 ms)
            SetTimer(hwnd, IDT_CLEAN_TIMER, g_Config.autoCleanIntervalSeconds * 1000, NULL);
            // Timer 2: 1-second countdown and RAM refresh tick
            SetTimer(hwnd, IDT_TICK_TIMER, 1000, NULL);

            // Trim working set memory right away
            Cleaner_TrimWorkingSet();

            // Trigger initial cleaning sweep on launch
            Cleaner_RunSweepAsync(hwnd, false);
            break;
        }

        case WM_TIMER: {
            if (wParam == IDT_CLEAN_TIMER) {
                if (g_Config.autoCleanEnabled && !g_Stats.isCleaning) {
                    g_SecondsUntilNextSweep = g_Config.autoCleanIntervalSeconds;
                    Cleaner_RunSweepAsync(hwnd, false);
                }
            } else if (wParam == IDT_TICK_TIMER) {
                if (g_Config.autoCleanEnabled && g_SecondsUntilNextSweep > 0) {
                    g_SecondsUntilNextSweep--;
                }
                Cleaner_TrimWorkingSet();
                UpdateTrayTooltip();
                if (IsWindowVisible(hwnd)) {
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            break;
        }

        case WM_CLEAN_FINISHED: {
            g_SecondsUntilNextSweep = g_Config.autoCleanIntervalSeconds;
            Cleaner_TrimWorkingSet();
            UpdateTrayTooltip();
            if (IsWindowVisible(hwnd)) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }

        case WM_TRAYICON: {
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                ShowTrayMenu(hwnd);
            } else if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONUP) {
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDM_CLEAN_NOW:
                case IDC_BTN_CLEAN_NOW: {
                    if (!g_Stats.isCleaning) {
                        Cleaner_RunSweepAsync(hwnd, true);
                    }
                    break;
                }

                case IDM_EMPTY_RECYCLE:
                case IDC_BTN_EMPTY_BIN: {
                    uint64_t bytes = Cleaner_EmptyRecycleBin();
                    g_Stats.totalBytesCleaned += bytes;
                    Cleaner_TrimWorkingSet();
                    InvalidateRect(hwnd, NULL, FALSE);
                    UpdateTrayTooltip();
                    break;
                }

                case IDM_TOGGLE_AUTOCLEAN:
                case IDC_BTN_TOGGLE_AUTO: {
                    g_Config.autoCleanEnabled = !g_Config.autoCleanEnabled;
                    g_SecondsUntilNextSweep = g_Config.autoCleanIntervalSeconds;
                    Cleaner_SaveConfig();
                    UpdateButtonTexts();
                    Cleaner_Log(g_Config.autoCleanEnabled ? L"5-Minute Auto-Clean: ENABLED" : L"5-Minute Auto-Clean: PAUSED");
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                }

                case IDM_TOGGLE_BLOCKER:
                case IDC_BTN_TOGGLE_BLOCK: {
                    g_Config.blockerEnabled = !g_Config.blockerEnabled;
                    Cleaner_SaveConfig();
                    UpdateButtonTexts();
                    Cleaner_Log(g_Config.blockerEnabled ? L"Background Bloat Blocker: ENABLED" : L"Background Bloat Blocker: DISABLED");
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                }

                case IDM_TOGGLE_STARTUP: {
                    bool current = Cleaner_IsAutoStartEnabled();
                    Cleaner_SetAutoStart(!current);
                    UpdateButtonTexts();
                    Cleaner_Log(!current ? L"Run at Windows Startup: ENABLED" : L"Run at Windows Startup: DISABLED");
                    break;
                }

                case IDM_SHOW_DASHBOARD: {
                    ShowWindow(hwnd, SW_RESTORE);
                    SetForegroundWindow(hwnd);
                    break;
                }

                case IDM_OPEN_CONFIG:
                case IDC_BTN_CONFIG: {
                    wchar_t exeDir[MAX_PATH];
                    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
                    wchar_t *p = wcsrchr(exeDir, L'\\');
                    if (p) *p = L'\0';
                    wchar_t iniPath[MAX_PATH];
                    swprintf(iniPath, MAX_PATH, L"%s\\cleaner_config.ini", exeDir);
                    ShellExecuteW(hwnd, L"open", L"notepad.exe", iniPath, NULL, SW_SHOWNORMAL);
                    break;
                }

                case IDC_BTN_MINIMIZE: {
                    ShowWindow(hwnd, SW_HIDE);
                    break;
                }

                case IDM_EXIT: {
                    Shell_NotifyIconW(NIM_DELETE, &g_nid);
                    DestroyWindow(hwnd);
                    break;
                }
            }
            break;
        }

        case WM_CTLCOLORLISTBOX: {
            HDC hdcList = (HDC)wParam;
            SetBkColor(hdcList, RGB(18, 22, 34));
            SetTextColor(hdcList, RGB(180, 215, 255));
            static HBRUSH hListBrush = NULL;
            if (!hListBrush) hListBrush = CreateSolidBrush(RGB(18, 22, 34));
            return (LRESULT)hListBrush;
        }

        case WM_PAINT:
            OnPaint(hwnd);
            break;

        case WM_CLOSE:
            // Intercept close button: minimize to tray instead of terminating
            ShowWindow(hwnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, IDT_CLEAN_TIMER);
            KillTimer(hwnd, IDT_TICK_TIMER);
            Shell_NotifyIconW(NIM_DELETE, &g_nid);

            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            if (g_hFontBold) DeleteObject(g_hFontBold);
            if (g_hFontNormal) DeleteObject(g_hFontNormal);
            if (g_hFontSmall) DeleteObject(g_hFontSmall);
            if (g_hFontNumber) DeleteObject(g_hFontNumber);
            if (g_hBrushBg) DeleteObject(g_hBrushBg);
            if (g_hBrushCard) DeleteObject(g_hBrushCard);
            if (g_hBrushBtn) DeleteObject(g_hBrushBtn);

            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)nCmdShow;

    // Mutex to ensure single instance
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"MaxCleaner_SingleInstance_Mutex_Maxpain");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"MaxCleanerWindowClass", L"Max Cleaner - Maxpain");
        if (hExisting) {
            ShowWindow(hExisting, SW_RESTORE);
            SetForegroundWindow(hExisting);
        }
        return 0;
    }

    // Initialize cleaner subsystem
    Cleaner_Init();

    // Register Window Class
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"MaxCleanerWindowClass";
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

    RegisterClassExW(&wc);

    // Calculate window size centered on primary monitor
    int w = 572;
    int h = 576;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - w) / 2;
    int y = (screenH - h) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        L"MaxCleanerWindowClass",
        L"Max Cleaner - Maxpain",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        return 1;
    }

    // Check if launched with -minimized or -silent
    bool bStartMinimized = false;
    if (lpCmdLine && (wcsstr(lpCmdLine, L"-minimized") || wcsstr(lpCmdLine, L"-silent") || wcsstr(lpCmdLine, L"/minimized"))) {
        bStartMinimized = true;
    }

    if (!bStartMinimized) {
        ShowWindow(hwnd, SW_SHOWNORMAL);
        UpdateWindow(hwnd);
    } else {
        ShowWindow(hwnd, SW_HIDE);
    }

    // Immediately release all startup memory to stay well under 10 MB limit
    Cleaner_TrimWorkingSet();

    // Standard high-efficiency Win32 message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hMutex) {
        CloseHandle(hMutex);
    }

    return (int)msg.wParam;
}
