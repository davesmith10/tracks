# Windows-Native C++ Toolchain

Command-line C++ development environment on Windows 11 using MSVC. No IDE.

Installed: **2026-02-15**

---

## Installed Components

| Component | Version | Install Method |
|-----------|---------|----------------|
| VS Build Tools 2022 | 17.14.26 | `winget install Microsoft.VisualStudio.2022.BuildTools` |
| MSVC (cl.exe) | 19.44.35222 | VS Build Tools workload `VCTools` |
| Windows 11 SDK | 10.0.22621.0, 10.0.26100.0 | VS Build Tools component |
| CMake | 4.2.3 | `winget install Kitware.CMake` |
| Ninja | 1.13.2 | `winget install Ninja-build.Ninja` |
| vcpkg | 2025-12-16 | `git clone` + `bootstrap-vcpkg.bat` |

---

## File Locations

### MSVC Build Tools

All MSVC tools live under the Build Tools install and are only available after running `vcvarsall.bat`.

```
C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\
├── VC\Auxiliary\Build\vcvarsall.bat            # Environment setup script
├── VC\Tools\MSVC\14.44.35207\
│   └── bin\Hostx64\x64\
│       ├── cl.exe                              # C/C++ compiler
│       ├── link.exe                            # Linker
│       ├── lib.exe                             # Static library manager
│       ├── nmake.exe                           # Microsoft make
│       └── dumpbin.exe                         # Binary inspector
└── Common7\Tools\LaunchDevCmd.bat              # Microsoft's dev prompt launcher
```

### Windows SDK

```
C:\Program Files (x86)\Windows Kits\10\
├── Include\10.0.26100.0\                       # Headers (windows.h, etc.)
├── Lib\10.0.26100.0\                           # Import libraries
└── bin\10.0.26100.0\x64\
    └── rc.exe                                  # Resource compiler
```

### Other Tools

```
C:\Program Files\CMake\bin\cmake.exe
C:\vcpkg\vcpkg.exe
C:\dev\devenv.cmd                               # Developer shell launcher
```

Ninja is installed via winget at:
```
%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe\ninja.exe
```

### Environment Variables

| Variable | Value | Scope |
|----------|-------|-------|
| `VCPKG_ROOT` | `C:\vcpkg` | User |
| `PATH` additions | `C:\vcpkg` | User |

---

## Usage

### Start a Developer Shell

MSVC tools are not on the global PATH. You must initialize the environment each session:

```cmd
C:\dev\devenv.cmd
```

This calls `vcvarsall.bat x64` and opens a shell with `cl`, `link`, `lib`, `nmake`, `rc`, and all SDK paths configured.

### Compile a Single File

```cmd
cl /EHsc /std:c++17 main.cpp
```

Common flags:
- `/EHsc` — standard C++ exception handling
- `/std:c++17` or `/std:c++20` — language standard
- `/W4` — high warning level
- `/O2` — optimize for speed
- `/Zi` — generate debug info
- `/Fe:name.exe` — set output executable name

### Build a CMake Project

```cmd
mkdir build && cd build
cmake -G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl ..
ninja
```

### Build a CMake Project with vcpkg

```cmd
mkdir build && cd build
cmake -G Ninja ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  ..
ninja
```

### Install a Library with vcpkg

```cmd
vcpkg install fmt
vcpkg install spdlog
vcpkg search <query>
vcpkg list
```

When using vcpkg manifest mode, add a `vcpkg.json` to your project root:

```json
{
  "dependencies": ["fmt", "spdlog"]
}
```

CMake will automatically install dependencies when `CMAKE_TOOLCHAIN_FILE` points to vcpkg.

### Inspect a Binary

```cmd
dumpbin /dependents myapp.exe
dumpbin /exports mylib.dll
dumpbin /headers myapp.exe
```

---

## Maintenance

### Update Build Tools

```cmd
winget upgrade Microsoft.VisualStudio.2022.BuildTools
```

Or use the VS Installer directly:
```cmd
"C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe" update ^
  --installPath "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools" ^
  --passive --norestart
```

### Update CMake / Ninja

```cmd
winget upgrade Kitware.CMake
winget upgrade Ninja-build.Ninja
```

### Update vcpkg

```cmd
cd C:\vcpkg
git pull
bootstrap-vcpkg.bat
```

### Check All Versions

From a developer shell (`devenv.cmd`):

```cmd
cl
cmake --version
ninja --version
vcpkg version
```

### MSVC Version Path Changes

When MSVC updates, the version directory under `VC\Tools\MSVC\` changes (e.g., `14.44.35207` to a newer number). This is handled automatically by `vcvarsall.bat` — no manual path updates needed.

---

## Verify Script

`C:\dev\verify.cmd` runs a quick check of all tools and compiles a test program. Run it from any terminal:

```cmd
C:\dev\verify.cmd
```

Expected output: version strings for cl, cmake, ninja, git, vcpkg, and `Hello from MSVC on Windows!`.
