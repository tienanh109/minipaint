@echo off
:: ==============================================================================
::  Author: tienanh109
::  Build script for MiniPaint. Builds for x86, x64, and ARM64 by default.
:: ==============================================================================
chcp 65001 > nul

:: --- 1. Find Visual Studio Installation ---
set "VS_BASE_PATH="
echo [SETUP] Searching for Visual Studio 2022 Build Tools...

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools" set "VS_BASE_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools" set "VS_BASE_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"

if not defined VS_BASE_PATH (
    echo [ERROR] Could not find Visual Studio 2022 Build Tools.
    echo         Please ensure it is installed in the default location.
    goto fail
)
echo [SETUP] Found Visual Studio at: "%VS_BASE_PATH%"

:: --- 2. Start Build Process ---
goto build_all

:: ==============================================================================
::                             BUILD DISPATCHER
:: ==============================================================================
:build_all
echo.
echo === STARTING ALL-ARCHITECTURE BUILD ===
call :build_arch x64
if errorlevel 1 goto fail
call :build_arch x86
if errorlevel 1 goto fail
call :build_arch arm64
if errorlevel 1 goto fail
echo.
echo ^<^<^< ALL BUILDS COMPLETED SUCCESSFULLY ^>^>^>
goto end

:: ==============================================================================
::                        CORE BUILD SUBROUTINE
::  This section contains the main build logic and is called for each arch.
:: ==============================================================================
:build_arch
set ARCH=%1
set VCVARS_PATH=
set VCVARS_ARGS=

if /I "%ARCH%" == "x64"   set "VCVARS_PATH=%VS_BASE_PATH%\VC\Auxiliary\Build\vcvars64.bat"
if /I "%ARCH%" == "x86"   set "VCVARS_PATH=%VS_BASE_PATH%\VC\Auxiliary\Build\vcvars32.bat"
if /I "%ARCH%" == "arm64" (
    set "VCVARS_PATH=%VS_BASE_PATH%\VC\Auxiliary\Build\vcvarsall.bat"
    set "VCVARS_ARGS=amd64_arm64"
)

if not exist "%VCVARS_PATH%" (
    echo [ERROR] Could not find vcvars script for %ARCH% at "%VCVARS_PATH%"
    exit /b 1
)

echo.
echo ^>==============================================================================
echo ^> BUILDING FOR: %ARCH%
echo ^>==============================================================================
echo.

:: 1. Setup Environment
echo [1/5] Setting up %ARCH% build environment...
if defined VCVARS_ARGS (
    call "%VCVARS_PATH%" %VCVARS_ARGS% > nul
) else (
    call "%VCVARS_PATH%" > nul
)
if errorlevel 1 (
    echo [ERROR] Failed to initialize the build environment for %ARCH%.
    exit /b 1
)

:: 2. Set Variables
set "SRC_DIR=src"
set "BUILD_DIR=build"
set "APP_NAME=MiniPaint_%ARCH%"
set "RES_FILE=%BUILD_DIR%\resource_%ARCH%.res"
set "OBJ_DIR=%BUILD_DIR%\obj_%ARCH%"

:: 3. Close Existing Instance
echo [2/5] Closing existing application instance (if any)...
taskkill /F /IM %APP_NAME%.exe > nul 2>&1

:: 4. Prepare Directories
echo [3/5] Preparing build directories...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

:: 5. Compile Resources
echo [4/5] Compiling resources for %ARCH%...
rc /nologo /fo "%RES_FILE%" resource.rc
if errorlevel 1 (
    echo [ERROR] Failed to compile 'resource.rc' for %ARCH%.
    exit /b 1
)

:: 6. Compile and Link
echo [5/5] Compiling and linking for %ARCH%...
cl /nologo /EHsc /O2 /W4 ^
   /Fo"%OBJ_DIR%\\" /Fe"%BUILD_DIR%\%APP_NAME%.exe" ^
   "%SRC_DIR%\main.cpp" "%RES_FILE%" ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comdlg32.lib comctl32.lib shell32.lib
if errorlevel 1 (
    echo [ERROR] Failed to compile or link source code for %ARCH%.
    exit /b 1
)

echo.
echo [SUCCESS] Build for %ARCH% completed.
echo           Output: %BUILD_DIR%\%APP_NAME%.exe
echo.
exit /b 0

:: ==============================================================================
::                             END OF SCRIPT
:: ==============================================================================
:fail
echo.
echo !!! BUILD PROCESS FAILED !!!
echo.
pause
exit /b 1

:end
echo.
echo You can find the executables in the '%BUILD_DIR%' directory.
pause
