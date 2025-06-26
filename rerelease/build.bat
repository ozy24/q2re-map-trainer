@echo off
echo Building Quake 2 Rerelease Game DLL...

REM Find Visual Studio Build Tools
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

REM Build the project
msbuild game.sln /p:Configuration=Release /p:Platform=x64

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful! 
    echo Game DLL created: ..\game_x64.dll
    echo.
) else (
    echo.
    echo Build failed. Check the output above for errors.
)
