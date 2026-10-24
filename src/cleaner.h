#ifndef CLEANER_H
#define CLEANER_H

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_BLOCKED_PROCESSES 64
#define MAX_PATH_LEN 512

typedef struct {
    uint64_t totalBytesCleaned;
    uint32_t totalFilesCleaned;
    uint32_t totalProcessesBlocked;
    uint32_t totalCleanPasses;
    uint32_t lastSweepCleanedFiles;
    uint64_t lastSweepCleanedBytes;
    uint32_t lastSweepBlockedProcs;
    uint32_t currentRamUsageKB;
    bool isCleaning;
} CleanerStats;

typedef struct {
    int autoCleanIntervalSeconds; // Default: 300 (5 mins)
    bool autoCleanEnabled;        // Default: true
    bool blockerEnabled;          // Default: true
    bool cleanRecycleBin;         // Default: true
    bool cleanBrowserCaches;      // Default: true (Active running apps are strictly preserved)
    bool cleanCrashDumps;         // Default: true
    int minFileAgeSeconds;        // Default: 14400 (4 hours - prevents deleting active app temp files)
    wchar_t blockedProcesses[MAX_BLOCKED_PROCESSES][64];
    int blockedCount;
} CleanerConfig;

extern CleanerStats g_Stats;
extern CleanerConfig g_Config;

// Initialize cleaner configuration & paths
void Cleaner_Init(void);

// Run a cleaning sweep asynchronously on a background thread
void Cleaner_RunSweepAsync(HWND hwndNotify, bool bManual);

// Check if a process is currently running by executable name
bool Cleaner_IsProcessRunning(const wchar_t *processExeName);

// Block unnecessary/bloat background applications
uint32_t Cleaner_BlockUnnecessaryApps(void);

// Empty recycle bin safely without prompt or sounds
uint64_t Cleaner_EmptyRecycleBin(void);

// Trim working set memory of Max Cleaner to strictly stay < 10 MB RAM
void Cleaner_TrimWorkingSet(void);

// Query own RAM working set in Kilobytes
uint32_t Cleaner_GetOwnMemoryUsageKB(void);

// Check if Max Cleaner is registered to start on Windows boot
bool Cleaner_IsAutoStartEnabled(void);

// Enable or disable Windows startup registration
bool Cleaner_SetAutoStart(bool bEnable);

// Load and save configuration from INI
void Cleaner_LoadConfig(void);
void Cleaner_SaveConfig(void);

// Log message callback definition
typedef void (*CleanerLogCallback)(const wchar_t *message);
void Cleaner_SetLogCallback(CleanerLogCallback cb);
void Cleaner_Log(const wchar_t *format, ...);

#endif // CLEANER_H
