# Quake 2 Rerelease Build Setup

This guide will help you set up the build environment for the Quake 2 rerelease source code to use as a foundation for your Rocket Arena 2 port.

## Prerequisites

### Required Software
1. **Visual Studio Build Tools 2022** (or Visual Studio Community 2022)
2. **Git** (already installed)

## Setup Instructions

### 1. Install Visual Studio Build Tools

**Download and Install:**
- Visit: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
- Download "Build Tools for Visual Studio 2022"
- Run the installer

**Required Components:**
- ✅ C++ build tools workload
- ✅ MSVC v143 - VS 2022 C++ x64/x86 build tools  
- ✅ Windows 11 SDK (latest version)
- ✅ CMake tools for Visual Studio

### 2. Dependencies Status
Dependencies are already set up in this directory:
- ✅ `fmt/` - fmt library for string formatting
- ✅ `json/` - jsoncpp library for JSON parsing

### 3. Building the Project

#### Option A: Using the Build Script
```cmd
# Run the build script
.\build.bat
```

#### Option B: Using Visual Studio Code
1. Open this folder in VS Code
2. Press `Ctrl+Shift+P`
3. Type "Tasks: Run Task"
4. Select "Build Quake 2 Rerelease DLL"

#### Option C: Command Line (after installing Build Tools)
```cmd
# Setup build environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

# Build the project
msbuild game.sln /p:Configuration=Release /p:Platform=x64
```

### 4. Output
After successful build, you'll find:
- `../game_x64.dll` - The compiled game DLL
- This DLL is your 1:1 copy of the original Quake 2 rerelease game logic

## Next Steps for Rocket Arena 2 Port

Once you have the base DLL building successfully:

1. **Study the Code Structure:**
   - `g_*.cpp` - Game logic files
   - `p_*.cpp` - Player-related functionality
   - `m_*.cpp` - Monster/AI code
   - `ctf/` - Capture the Flag mode

2. **Create RA2 Modifications:**
   - Arena-based gameplay system
   - Weapon spawn mechanics
   - Tournament/match system
   - RA2-specific weapons and items

3. **Test Your Changes:**
   - Build the DLL
   - Copy to your Quake 2 rerelease mod folder
   - Launch with `+set game yourmodname`

## Troubleshooting

### Build Errors
- Ensure Visual Studio Build Tools are properly installed
- Check that all required components are selected
- Verify the vcvarsall.bat path in build.bat

### Missing Dependencies
If you get errors about missing fmt or jsoncpp:
- The dependencies are already downloaded in `fmt/` and `json/` directories
- Ensure these directories contain the source code

### VS Code Integration
- Install the C/C++ extension for better IntelliSense
- The tasks.json is configured for easy building from VS Code

## Resources
- [Original Quake 2 Rerelease README](README.md)
- [id Software Quake 2 Rerelease Repository](https://github.com/id-Software/quake2-rerelease-dll) 