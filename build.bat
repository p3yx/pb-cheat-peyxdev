@echo off
REM ============================================================
REM  Build PB-CHEAT - 32-bit (PointBlank.exe adalah 32-bit/WOW64)
REM  Toolchain: C:\Users\Peyx\tools\mingw32 (WinLibs i686 ucrt posix)
REM ============================================================
setlocal EnableExtensions
set "CXX=C:\Users\Peyx\tools\mingw32\bin\i686-w64-mingw32-g++.exe"

if not exist "%CXX%" (
  echo [!] Toolchain 32-bit tidak ditemukan: %CXX%
  pause
  exit /b 1
)

echo [*] Compiler: %CXX%
set "FLAGS=-std=c++17 -O2 -s -static -static-libgcc -static-libstdc++"

echo [*] Compile pb-cheat.dll (32-bit) ...
"%CXX%" %FLAGS% -shared -o pb-cheat.dll ^
  src/dllmain.cpp src/Config.cpp src/Memory.cpp src/Structs.cpp ^
  src/Game.cpp src/ESP.cpp src/Menu.cpp src/Patches.cpp src/ReloadFast.cpp ^
  -luser32 -lgdi32 -lwinmm
if errorlevel 1 goto fail

echo [*] Compile injector.exe (32-bit) ...
"%CXX%" %FLAGS% -o injector.exe injector/injector.cpp -luser32
if errorlevel 1 goto fail

echo [*] Compile launcher.exe (32-bit, 1-klik) ...
"%CXX%" %FLAGS% -mwindows -o launcher.exe launcher.cpp -luser32 -lshell32
if errorlevel 1 goto fail

echo.
echo [+] SELESAI (32-bit):
echo     launcher.exe   <- KLIK INI (auto-UAC + inject)
echo     pb-cheat.dll   <- DLL cheat + floating menu
echo     injector.exe   <- injector manual (cadangan)
echo     config\offsets.cfg
pause
exit /b 0

:fail
echo [!] COMPILE GAGAL.
pause
exit /b 1
