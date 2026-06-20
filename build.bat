@echo off
rem Debug build (~8 MB DLL, debug cvars, large PDB). Optional: build.bat [Platform] [Toolset]
setlocal EnableExtensions

if not defined MUFFMODE_BUILD_CONFIG set "MUFFMODE_BUILD_CONFIG=Debug"

set "ROOT=%~dp0"
pushd "%ROOT%" >nul

set "SOLUTION=src\game.sln"
if not exist "%SOLUTION%" (
    echo [ERROR] Could not find "%SOLUTION%".
    popd >nul
    exit /b 1
)

if not exist "%ROOT%vcpkg\scripts\buildsystems\msbuild\vcpkg.props" (
    echo [ERROR] vcpkg submodule is missing or not initialized.
    echo         Run: git submodule update --init --recursive
    popd >nul
    exit /b 1
)

if not exist "%ROOT%vcpkg\vcpkg.exe" (
    echo [ERROR] vcpkg is not bootstrapped.
    echo         Run: .\scripts\setup-vcpkg.ps1
    popd >nul
    exit /b 1
)

if not exist "%ROOT%src\vcpkg_installed\" (
    echo [BUILD] Installing vcpkg manifest dependencies...
    set "VCPKG_ROOT=%ROOT%vcpkg"
    pushd "%ROOT%src" >nul
    "%VCPKG_ROOT%\vcpkg.exe" install --triplet x64-windows-static
    if errorlevel 1 (
        popd >nul
        popd >nul
        exit /b 1
    )
    popd >nul
)

set "CONFIG=%MUFFMODE_BUILD_CONFIG%"

set "PLATFORM=%~1"
if "%PLATFORM%"=="" set "PLATFORM=x64"

set "TOOLSET=%~2"
if "%TOOLSET%"=="" set "TOOLSET=v143"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDEVCMD="

if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find Common7\Tools\VsDevCmd.bat`) do (
        set "VSDEVCMD=%%I"
    )
)

if not defined VSDEVCMD if exist "%ProgramFiles%\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" (
    set "VSDEVCMD=%ProgramFiles%\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
)
if not defined VSDEVCMD if exist "%ProgramFiles%\Microsoft Visual Studio\17\Community\Common7\Tools\VsDevCmd.bat" (
    set "VSDEVCMD=%ProgramFiles%\Microsoft Visual Studio\17\Community\Common7\Tools\VsDevCmd.bat"
)
if not defined VSDEVCMD if exist "%ProgramFiles%\Microsoft Visual Studio\17\BuildTools\Common7\Tools\VsDevCmd.bat" (
    set "VSDEVCMD=%ProgramFiles%\Microsoft Visual Studio\17\BuildTools\Common7\Tools\VsDevCmd.bat"
)

if not defined VSDEVCMD (
    echo [ERROR] Could not locate VsDevCmd.bat.
    echo         Install Visual Studio Build Tools with C++ support.
    popd >nul
    exit /b 1
)

call "%VSDEVCMD%" -host_arch=x64 -arch=x64 >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialize Visual Studio build environment.
    popd >nul
    exit /b 1
)

where msbuild >nul 2>nul
if errorlevel 1 (
    echo [ERROR] msbuild was not found after environment setup.
    popd >nul
    exit /b 1
)

if exist "%ROOT%game_x64.exp" del "%ROOT%game_x64.exp"

echo [BUILD] Solution  : %SOLUTION%
echo [BUILD] Config    : %CONFIG%
echo [BUILD] Platform  : %PLATFORM%
echo [BUILD] Toolset   : %TOOLSET%
echo [BUILD] VsDevCmd  : %VSDEVCMD%
echo.

msbuild "%SOLUTION%" /m /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /p:PlatformToolset=%TOOLSET%
set "BUILD_EXIT=%ERRORLEVEL%"

popd >nul
exit /b %BUILD_EXIT%
