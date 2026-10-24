# ⚡ Max Cleaner
### *Ultimate Ultra-Lightweight PC Performance & Junk Cleaner*
**Publisher:** Maxpain  
**Software Name:** Max Cleaner  
**Binary Size:** ~75 KB  
**RAM Consumption:** 0.43 MB (Stealth/Tray) | ~1.85 MB (Dashboard)  
**Strict Memory Cap:** < 10.0 MB RAM  
**CPU Impact:** < 0.1% (Zero Frame Drop Idle I/O Engine)  

---

## 🌟 Overview

**Max Cleaner** is an ultra-optimized, high-performance Windows PC optimizer and background clutter suppressor engineered specifically for gamers, power users, and everyday performance enthusiasts.

Unlike traditional PC cleaners that bloat your system with 150MB+ of background RAM, constant telemetry, and disk-thrashing spikes that cause sudden in-game micro-stutters and frame drops, **Max Cleaner is written in pure native C using bare-metal Win32 APIs**. It compiles to a featherweight **75 KB executable**, uses **less than 1 to 2 MB of RAM**, and runs on the Windows NT Kernel's background I/O throttle so your games and foreground apps always receive 100% hardware priority.

---

## 🚀 Key Features

### 1. 🛡️ Strict < 10 MB RAM Guarantee (Actual: 0.43 – 1.85 MB)
* Engineered with zero external runtime bloat (no .NET runtime, no Electron, no WebView2).
* Actively flushes unused pages using `EmptyWorkingSet()` and Windows working set trimming after every sweep.
* **Stealth / Background Mode:** Uses only **0.43 MB (430 KB)** of RAM.
* **Full Graphical Dashboard:** Uses only **1.85 MB** of RAM — more than 5x below the 10 MB limit.

### 2. 🎮 Zero Frame Drop Anti-Lag Engine
* Background cleaning sweeps run inside a dedicated background worker thread configured with:
  ```c
  SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
  ```
* Windows Kernel automatically drops CPU scheduling to lowest priority and sets **Disk I/O Priority to IDLE / VERY LOW**.
* The storage controller will **only** process Max Cleaner read/delete operations when storage channels are completely idle, completely eliminating disk starvation and game frame stutters.
* Features micro-yielding (`Sleep(1)` every 40 files) to give CPU time slices immediately back to foreground applications.

### 3. ⏱️ Autonomous 5-Minute Auto-Clean Cycle
* Automatically cleans junk and clutter every **5 minutes** (300 seconds) in the background without disturbing the user.
* Includes a real-time countdown timer in both the system tray tooltip and the dashboard.
* Can be easily paused or resumed anytime with a single click.

### 4. 🧹 Deep Clutter & Junk Elimination
Safely purges non-essential clutter while strictly protecting user data, logins, and open projects:
* **User Temp Files:** `%TEMP%` and `%LOCALAPPDATA%\Temp\*`
* **Windows System Temp:** `%WINDIR%\Temp\*`
* **Windows Update Cache:** `%WINDIR%\SoftwareDistribution\Download\*`
* **Crash Dumps & Error Reports:** `%LOCALAPPDATA%\CrashDumps\*` and Windows Error Reporting (WER) logs.
* **Browser Caches:** Google Chrome, Microsoft Edge, Brave, Firefox, Discord Cache, and Steam Web Cache (caches only; passwords, history, and cookies are 100% preserved).
* **Recycle Bin:** Clean silent purge using `SHEmptyRecycleBinW()`.
* **Safe-Deletion Threshold:** Never touches files created/modified within the last 60 seconds to prevent race conditions with open software.
* **Reparse Point Protection:** Strictly skips junction points and symlinks to prevent infinite loops.

### 5. 🚫 Unnecessary Background App Blocker
* Actively checks and suppresses resource-draining background processes, stuck update daemons, telemetry collectors, and adware monitors:
  * `CompatTelRunner.exe` (Windows Diagnostics / Telemetry runner)
  * `devicecensus.exe` (Census telemetry daemon)
  * `wermgr.exe` (Stuck error reporting daemons)
  * `CCleaner64.exe` / `CCleanerReactivator.exe` (Third-party bloat)
  * `OneDriveStandaloneUpdater.exe`, `GoogleUpdate.exe`, `MicrosoftEdgeUpdate.exe` (Stuck background auto-launchers)
  * `jusched.exe`, `AdobeUpdateService.exe`, `AGSService.exe`
* **Whitelist Shield:** Critical Windows operating system processes (`csrss.exe`, `explorer.exe`, `dwm.exe`, `svchost.exe`, `lsass.exe`, `services.exe`, etc.) are hardcoded into a protected whitelist and can never be terminated.
* Fully customizable through `cleaner_config.ini` in the `[BlockedProcesses]` section.

### 6. 🔌 Automatic Startup on Windows Boot
* Registers securely in `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`.
* Launches with `-minimized` flag directly into the Windows System Tray on boot without popping up windows or slowing down boot time.
* Toggleable instantly from the System Tray menu or Dashboard.

### 7. 🎨 Custom Logo, Multi-Resolution Icon & Dark UI
* Custom-crafted multi-resolution application icon (16x16, 24x24, 32x32, 48x48, 64x64, 128x128, 256x256) embedded directly into the `.exe`.
* Windows Explorer and Taskbar display the official Max Cleaner icon.
* Embedded Windows Version Information showing **Maxpain** as the official publisher and **Max Cleaner** as the product name.
* Sleek Cyber-Dark Dashboard with real-time RAM meter, files eliminated counter, total MB/GB freed, and live event log.

---

## 📁 Project Structure

```
Cleaner/
├── bin/
│   ├── MaxCleaner.exe         # Production binary (75 KB, Publisher: Maxpain)
│   └── cleaner_config.ini     # Live settings and blocklist
├── assets/
│   ├── app.ico                # Multi-resolution application icon
│   ├── logo_256.png           # High-resolution logo
│   └── generate_icon.ps1      # Icon generator script
├── src/
│   ├── main.c                 # Win32 UI, System Tray, Event Loop & GDI Rendering
│   ├── cleaner.c              # Low-I/O Cleaning Engine & Process Suppressor
│   ├── cleaner.h              # Cleaner declarations & stats structures
│   ├── resource.h             # Resource IDs & Menu definitions
│   ├── resource.rc            # App icon, publisher metadata & version info
│   └── app.manifest           # Modern styling & High-DPI manifest
├── build.bat                  # 1-Click compiler script (MinGW GCC)
├── Start_MaxCleaner.bat       # Launcher shortcut
└── README.md                  # Complete documentation
```

---

## 🛠️ How to Run & Use

### Method 1: Desktop Shortcut
A shortcut named **Max Cleaner** has been created on your Desktop. Simply double-click it to launch!

### Method 2: System Tray & Minimized Mode
1. When launched, Max Cleaner places a sleek icon in your Windows Notification Area / System Tray.
2. Hovering over the icon displays:
   ```
   Max Cleaner by Maxpain
   RAM: 1.85 MB (<10MB Limit)
   Cleaned: 142.0 MB | Blocked: 0
   ```
3. **Right-Click Menu Options:**
   * ⚡ **Clean Now (Turbo Sweep):** Triggers an immediate clean sweep.
   * 🗑️ **Empty Recycle Bin:** Cleans recycle bin across all drives silently.
   * ⏱️ **Auto-Clean Every 5 Mins:** Toggle periodic cleaning.
   * 🛡️ **Block Unnecessary Apps:** Toggle process suppressor.
   * 🚀 **Run on Windows Startup:** Toggle automatic boot start.
   * 📊 **Open Dashboard:** Displays full performance stats & log.
   * ⚙️ **Edit Config / Blocklist:** Opens `cleaner_config.ini` in Notepad.
   * ❌ **Exit Max Cleaner:** Closes the program.

### Method 3: Dashboard Controls
* Double-click the tray icon to open the Dashboard.
* The dashboard displays live metrics:
  * **Memory / RAM Usage:** Live working set verification.
  * **Total Junk Cleaned:** Running total of bytes freed.
  * **Files Eliminated:** Clutter count.
  * **Unnecessary Apps Blocked:** Suppressed background apps count.
  * **Next Auto-Clean:** 5-minute countdown clock.
  * **CPU / FPS Impact:** Confirms background low-I/O mode.
* Clicking the **[X] Close** button safely minimizes Max Cleaner back to the system tray so your PC remains protected 24/7 without taskbar clutter.

---

## ⚙️ Customization (`cleaner_config.ini`)

Edit `bin\cleaner_config.ini` to adjust settings:

```ini
[Settings]
AutoCleanIntervalSeconds=300   ; Interval in seconds (300 = 5 minutes)
AutoCleanEnabled=1            ; 1 = Enabled, 0 = Disabled
BlockerEnabled=1              ; 1 = Suppress unwanted background apps
StartWithWindows=1            ; 1 = Run automatically on boot
CleanRecycleBin=1             ; 1 = Empty recycle bin during auto sweeps
CleanBrowserCaches=1          ; 1 = Clean browser caches
MinFileAgeSeconds=60          ; Skip files created within last 60 seconds

[BlockedProcesses]
Process1=CompatTelRunner.exe
Process2=devicecensus.exe
Process3=wermgr.exe
Process4=gamebarftserver.exe
Process5=CCleaner64.exe
Process6=OneDriveStandaloneUpdater.exe
; Add more processes here...
```

---

## 🔨 Compiling From Source

To rebuild the executable at any time:
1. Double-click `build.bat` in the project root.
2. The script compiles resources using `windres` and links with `gcc -O2 -s -municode -mwindows`.
3. The resulting `MaxCleaner.exe` (~75 KB) is saved in the `bin\` folder.
