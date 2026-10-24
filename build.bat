@echo off
setlocal
echo ========================================================
echo   Compiling Max Cleaner by Maxpain
echo ========================================================

where gcc >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: GCC compiler not found in PATH.
    pause
    exit /b 1
)

cd /d "%~dp0"
if not exist "bin" mkdir bin

echo [1/3] Compiling Windows Resources (App Icon + Version Info)...
windres .\src\resource.rc -O coff -o .\src\resource.o
if %errorlevel% neq 0 (
    echo Failed to compile resources!
    pause
    exit /b 1
)

echo [2/3] Compiling Native C Binary (High-Optimization -O2 -s)...
gcc -O2 -s -municode -mwindows .\src\main.c .\src\cleaner.c .\src\resource.o -o .\bin\MaxCleaner.exe -lcomctl32 -lpsapi -lshell32 -lole32
if %errorlevel% neq 0 (
    echo Compilation failed!
    pause
    exit /b 1
)

echo [3/3] Copying Configuration Files...
copy /y .\cleaner_config.ini .\bin\cleaner_config.ini >nul

echo.
echo ========================================================
echo   SUCCESS: bin\MaxCleaner.exe built successfully!
echo   Publisher: Maxpain
echo   RAM Target: Strict less than 10 MB (Runs at ~0.5 - 2.0 MB)
echo ========================================================
pause
