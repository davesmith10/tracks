# Plan: Windows-Native Command-Line C++ Development Environment

## Context

Set up a fully command-line-driven C++ development environment on Windows 11, with no IDE. The goal is to build Win32 and console applications using MSVC, CMake + Ninja, and vcpkg. This complements an existing WSL2 setup and does **not** use WSL.

## Starting State

- **Already installed:** Git, Python (Store), winget
- **Not installed:** No compiler, no Windows SDK, no build tools, no CMake, no Ninja, no vcpkg
- Visual Studio Installer stub exists but nothing is installed through it

## Design Decisions

These were chosen during the planning phase:

| Decision | Choice | Alternatives Considered |
|----------|--------|------------------------|
| Compiler | MSVC (`cl.exe`) | LLVM/Clang, MinGW-w64 (GCC), MSVC + Clang both |
| Build system | CMake + Ninja | CMake + NMake, MSBuild, CMake only |
| Application type | Win32 / Console apps | Win32 + WinRT/UWP |
| Package manager | vcpkg | Conan, none |

---

## Installation Steps

### Step 1: Install Visual Studio Build Tools 2022 (MSVC + Windows SDK)

The core step. "Build Tools" is the no-IDE package that provides `cl.exe`, `link.exe`, `lib.exe`, `nmake.exe`, the C/C++ runtime, and the Windows SDK.

```
winget install Microsoft.VisualStudio.2022.BuildTools
```

Then add the required workload and components. The `--passive` flag requires an **elevated (admin) shell**:

```cmd
"C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe" install ^
  --productId Microsoft.VisualStudio.Product.BuildTools ^
  --channelId VisualStudio.17.Release ^
  --add Microsoft.VisualStudio.Workload.VCTools ^
  --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
  --add Microsoft.VisualStudio.Component.Windows11SDK.22621 ^
  --includeRecommended ^
  --passive --norestart
```

**Implementation note:** The original plan used `vs_installer.exe modify`, but the correct approach is `setup.exe install`. The `--passive` flag requires running from an elevated command prompt — it will fail with exit code 5007 from a non-admin shell. Without `--passive`, the GUI installer opens and you can click through manually.

**What this provides:**
- `cl.exe` (C/C++ compiler)
- `link.exe` (linker)
- `lib.exe` (static library manager)
- `dumpbin.exe` (binary inspection)
- `nmake.exe` (Microsoft make)
- Windows SDK headers, libraries, and tools (`rc.exe`, etc.)
- MSVC C/C++ runtime libraries

### Step 2: Install CMake

```
winget install Kitware.CMake --accept-package-agreements --accept-source-agreements
```

### Step 3: Install Ninja

```
winget install Ninja-build.Ninja --accept-package-agreements --accept-source-agreements
```

**Implementation note:** Steps 2 and 3 are independent of Step 1 and can run in parallel while Build Tools installs.

### Step 4: Install vcpkg

```
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
```

Bootstrap using the full path (required when not in the vcpkg directory):

```cmd
C:\vcpkg\bootstrap-vcpkg.bat
```

Set the environment variable and add to PATH:

```cmd
setx VCPKG_ROOT "C:\vcpkg"
```

Add `C:\vcpkg` to user PATH via PowerShell:

```powershell
[Environment]::SetEnvironmentVariable('Path', [Environment]::GetEnvironmentVariable('Path', 'User') + 'C:\vcpkg;', 'User')
```

### Step 5: Set up the "Developer Command Prompt" equivalent

MSVC tools (`cl.exe`, `link.exe`) are **not** added to the global PATH. Microsoft provides `vcvarsall.bat` to set up the environment per-session.

Create `C:\dev\devenv.cmd`:
```cmd
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
echo MSVC x64 environment ready.
cmd /k
```

### Step 6: Verify the installation

Create `C:\dev\verify.cmd` to test all tools:
```cmd
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

echo === MSVC Compiler ===
cl 2>&1
echo.
echo === Linker ===
link 2>&1 | findstr "Microsoft"
echo.
echo === CMake ===
cmake --version
echo.
echo === Ninja ===
ninja --version
echo.
echo === Git ===
git --version
echo.
echo === vcpkg ===
C:\vcpkg\vcpkg.exe version
echo.
echo === Compile Test ===
echo #include ^<stdio.h^> > %TEMP%\test.cpp
echo int main() { printf("Hello from MSVC on Windows!\n"); return 0; } >> %TEMP%\test.cpp
cl /nologo /Fe:%TEMP%\test.exe %TEMP%\test.cpp /link /nologo
%TEMP%\test.exe
```

### Step 7: (Optional) Additional CLI tools

| Tool | Install | Purpose |
|------|---------|---------|
| `clang-format` | `winget install LLVM.LLVM` | Code formatting |
| `cppcheck` | `winget install Cppcheck.Cppcheck` | Static analysis |
| `doxygen` | `winget install DimitriVanHeesch.Doxygen` | Documentation |

---

## Verification Results (2026-02-15)

All checks passed:

```
=== MSVC Compiler ===
Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35222 for x64

=== Linker ===
Microsoft (R) Incremental Linker Version 14.44.35222.0

=== CMake ===
cmake version 4.2.3

=== Ninja ===
1.13.2

=== Git ===
git version 2.53.0.windows.1

=== vcpkg ===
vcpkg package management program version 2025-12-16

=== Compile Test ===
Hello from MSVC on Windows!
```

## Lessons Learned

1. **`setup.exe` not `vs_installer.exe`** — Use `setup.exe install` with `--productId` and `--channelId` to install Build Tools from the command line. `vs_installer.exe modify` is for modifying existing installations.

2. **Elevation required for `--passive`** — The `--passive` and `--quiet` flags require the command to be run from an elevated (Run as Administrator) shell. Without elevation, the installer exits with code 5007.

3. **`bootstrap-vcpkg.bat` needs a full path** — When running from a bash-like shell (Git Bash, MSYS2), calling the batch file by name from `cd C:\vcpkg` may not work. Use the full path: `C:\vcpkg\bootstrap-vcpkg.bat`.

4. **PATH changes need a new shell** — Environment variable changes via `setx` or `[Environment]::SetEnvironmentVariable` only take effect in new terminal sessions, not the current one.

5. **Steps 2-4 are independent** — CMake, Ninja, and vcpkg installs don't depend on Build Tools and can run in parallel while the large Build Tools download completes.
