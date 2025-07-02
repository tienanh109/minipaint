@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
chcp 65001 > nul

set SRC_DIR=src
set BUILD_DIR=build
set APP_NAME=MiniPaint

echo [1/4] Preparing build directory...
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
del %BUILD_DIR%\%APP_NAME%.exe %BUILD_DIR%\*.res %BUILD_DIR%\*.obj > nul 2>&1

echo [2/4] Compiling resources...
rc /fo %BUILD_DIR%\resource.res resource.rc || goto error_rc

echo [3/4] Compiling and linking application...
cl /nologo /EHsc /O2 /W4 ^
    /I "%VCToolsInstallDir%atlmfc\include" ^
    /Fo%BUILD_DIR%\ /Fe%BUILD_DIR%\%APP_NAME%.exe ^
    %SRC_DIR%\main.cpp %BUILD_DIR%\resource.res ^
    /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comdlg32.lib comctl32.lib ole32.lib oleaut32.lib uuid.lib atls.lib || goto error_cl

echo [4/4] Build successful. Launching app...
start "" "%BUILD_DIR%\%APP_NAME%.exe"
goto end

:error_rc
echo [ERROR] Failed to compile 'resource.rc'
goto fail

:error_cl
echo [ERROR] Failed to compile/link source code
goto fail

:fail
pause

:end
