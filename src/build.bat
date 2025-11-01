@echo off
echo Building Quake 2 Rerelease Game DLL...

REM Find Visual Studio Build Tools
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

REM Build the project
msbuild game.sln /p:Configuration=Release /p:Platform=x64

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful! 
    echo Game DLL created: ..\dist\game_x64.dll
    
    REM Copy DLL to Quake 2 directory
    echo Copying game_x64.dll to Quake 2 directory...
    copy /Y "..\dist\game_x64.dll" "c:\Program Files (x86)\Steam\steamapps\common\Quake 2\rerelease\baseq2\"
    
    if %ERRORLEVEL% EQU 0 (
        echo DLL copied successfully!
        echo.
        echo Launching Quake 2...
        start "" "c:\Program Files (x86)\Steam\steamapps\common\Quake 2\rerelease\quake2ex_steam.exe" +set deathmatch 1 +set timelimit 10 +map q2dm1
    ) else (
        echo Warning: Failed to copy DLL. Check permissions.
    )
    
    echo.
) else (
    echo.
    echo Build failed. Check the output above for errors.
)