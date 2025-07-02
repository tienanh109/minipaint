@echo off
rem Switch Command Prompt to UTF-8 to display Unicode characters correctly
chcp 65001 > nul
cls

set SRC_DIR=src
set ASSET_DIR=assets
set BUILD_DIR=build
set APP_NAME=MiniPaint

echo.
echo ===========================================
echo      ENHANCED MINI PAINT BUILD SCRIPT
echo ===========================================
echo.

echo [1/5] Creating build directory...
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo [2/5] Cleaning previous build files...
del %BUILD_DIR%\%APP_NAME%.exe %BUILD_DIR%\*.res %BUILD_DIR%\*.obj > nul 2>&1

echo [3/5] Compiling resources...
rc.exe /fo%BUILD_DIR%\resource.res resource.rc
if errorlevel 1 goto error_rc

echo [4/5] Compiling and linking application...
rem ADDED atls.lib to the linker command to fix LNK1104
cl.exe /EHsc /W4 /I "%VCToolsInstallDir%atlmfc\include" /Fo%BUILD_DIR%\ /Fe%BUILD_DIR%\%APP_NAME%.exe %SRC_DIR%\main.cpp %BUILD_DIR%\resource.res /link user32.lib gdi32.lib comdlg32.lib comctl32.lib ole32.lib oleaut32.lib uuid.lib atls.lib
if errorlevel 1 goto error_cl

echo.
echo ===========================================
echo      BUILD SUCCESSFUL!
echo ===========================================
echo.
echo [5/5] Starting Enhanced Mini Paint from '%BUILD_DIR%' directory...
start "" "%BUILD_DIR%\%APP_NAME%.exe"
goto end

:error_rc
echo.
echo *** BUILD FAILED! ***
echo Could not compile resource file (resource.rc).
echo Please ensure rc.exe is in your path and asset files exist.
goto fail

:error_cl
echo.
echo *** BUILD FAILED! ***
echo Could not compile C++ source file (main.cpp).
echo Please check for code errors.
goto fail

:fail
echo.
pause

:end
