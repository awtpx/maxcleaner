#ifndef RESOURCE_H
#define RESOURCE_H

#define IDI_APP_ICON            101
#define IDR_TRAY_MENU           102

// Menu Commands
#define IDM_HEADER              2001
#define IDM_CLEAN_NOW           2002
#define IDM_EMPTY_RECYCLE       2003
#define IDM_TOGGLE_AUTOCLEAN    2004
#define IDM_TOGGLE_BLOCKER      2005
#define IDM_TOGGLE_STARTUP      2006
#define IDM_SHOW_DASHBOARD      2007
#define IDM_OPEN_CONFIG         2008
#define IDM_EXIT                2009

// Dashboard Control IDs
#define IDC_BTN_CLEAN_NOW       3001
#define IDC_BTN_EMPTY_BIN       3002
#define IDC_BTN_TOGGLE_AUTO     3003
#define IDC_BTN_TOGGLE_BLOCK    3004
#define IDC_BTN_CONFIG          3005
#define IDC_BTN_MINIMIZE        3006
#define IDC_LIST_LOG            3007

// Custom Messages
#define WM_TRAYICON             (WM_USER + 10)
#define WM_CLEAN_FINISHED       (WM_USER + 11)
#define WM_CLEAN_PROGRESS       (WM_USER + 12)

#endif // RESOURCE_H
