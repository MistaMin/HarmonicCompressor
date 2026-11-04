@echo off
REM Local Windows build helper for HarmonicCompressor (uncommitted, mirrors HybridEQ/build_win.bat).
REM Loads the MSVC x64 environment, then configures (if needed) and builds.
REM
REM   build_win.bat                 -> configure + build all targets
REM   build_win.bat --configure     -> configure only

setlocal

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialise the MSVC environment.
    exit /b 1
)

if not exist "build\CMakeCache.txt" (
    echo [HarmonicCompressor] Configuring...
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAAX_SDK_PATH=C:/Users/marco/Developer/aax-sdk-2-8-1
    if errorlevel 1 exit /b 1
)

if /i "%~1"=="--configure" exit /b 0

echo [HarmonicCompressor] Building all targets...
cmake --build build --config Release

exit /b %errorlevel%
