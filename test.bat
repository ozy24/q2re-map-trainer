@echo off
rem Build and run host-compiled trainer_logic unit tests (no engine).
setlocal EnableExtensions

set "ROOT=%~dp0"
pushd "%ROOT%" >nul

call "%ROOT%build.bat"
if errorlevel 1 (
    popd >nul
    exit /b 1
)

if not exist "%ROOT%dist\trainer_tests_x64.exe" (
    echo [ERROR] dist\trainer_tests_x64.exe was not built.
    popd >nul
    exit /b 1
)

echo.
echo [TEST] Running trainer_logic unit tests...
"%ROOT%dist\trainer_tests_x64.exe"
set "TEST_EXIT=%ERRORLEVEL%"

popd >nul
exit /b %TEST_EXIT%
