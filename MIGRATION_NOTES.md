# Windows x64 migration (VS2026)

The supported Windows solution is `build\vs2026\quake2.sln` (v145 toolset and Windows SDK).
All five projects have Debug and Release configurations for x64 and Win32.

## Build and run

From the repository root:

```bat
build\vs2026\build.bat Debug x64
build\vs2026\build.bat Release x64
```

The script discovers MSBuild using Visual Studio Installer's `vswhere` and returns its exit code.
Win32 remains available by passing `Win32` as the second argument.

x64 outputs go to `x64\Debug\` or `x64\Release\`: `quake2.exe`, `ref_gl.dll`,
`ref_soft.dll`, and `gamex64.dll`. The CTF module is in the configuration's `ctf\` folder.
Intermediate files are separate from Win32. Original VC6 files and Unix builds are unchanged.

Put legally owned original game PAK files in the output directory's `baseq2\` folder,
then launch `quake2.exe` with that output directory as the working directory. For CTF,
put its PAK files in the output directory's `ctf\` folder and pass `+set game ctf`.
The loader selects `gamex64.dll` and respects the active mod when checking development paths.
64-bit engines require 64-bit renderer, game, and GL-driver DLLs (including custom SGL drivers).

Bundled launchers can keep `quake2.exe`, `ref_gl.dll`, and `gamex64.dll` together in a
separate engine folder and use the installed game folder as the working directory.
After checking development and game-data paths, the game DLL loader also checks beside
the executable, or its matching mod subfolder (for example `ctf/gamex64.dll`). This avoids
copying engine DLLs over an installed x86 game. Release builds use the static C runtime;
Windows system libraries and an architecture-matching OpenGL driver are still required.

## Architecture changes

- x64 uses the existing C renderer/math implementations; x86 MASM is excluded.
- Pointer alignment, cache addresses, sorting, structure offsets, CRT file-search handles,
  console handles, and MCI audio callbacks use pointer-sized types or `offsetof`.
- Polygon vertex stepping uses the size of a float rather than the size of a pointer.
  Empty span lists are checked before accessing their coordinates.
- x87 precision-control calls are restricted to x86; SSE on x64 does not support that setting.
- Save pointer slots and relocation offsets use native pointer width. x64 save headers have a
  `-64` marker, rejecting Win32 saves even when the build dates match. Base-game saves can be
  reloaded after DLL ASLR changes. CTF x64 callback/movement fields also use relocation offsets.
  Saves still depend on the same module build and are not portable between architectures.
- Pointer truncation warnings C4302, C4311, and C4312 are errors in x64 configurations.

## Verification

All five projects build in Debug/Release for x64 and Win32. Runtime checks use locally installed
original game assets copied into ignored build output, never into tracked source files.
The x64 Debug software renderer loads `base1`, renders frames, saves, reloads, and exits.
A separate OpenGL launch (system `opengl32`) reloads that save with a different game DLL base
address and renders successfully. CTF loads its own x64 DLL and renders `base1`; normal CTF
forces deathmatch and disables manual savegames.

`build\vs2026\no_dialogs.cdb` captures CRT reports, invalid parameters, engine fatal errors,
modal error dialogs, access violations, stack overflows, fail-fast, and unhandled exceptions.
It prints diagnostics and terminates the debuggee rather than waiting for user attention.
For an x64 Debug build, from its output directory, use the Windows SDK debugger:

```bat
cdb -y . -G -cf ..\..\build\vs2026\no_dialogs.cdb quake2.exe +set vid_ref soft +set vid_fullscreen 0 +set logfile 2 +exec smoke.cfg
```

Use an automatic config that loads a map, waits for client connection/rendering, performs checks,
and quits. Check debugger output for `FAILURE:` and the game log for expected actions; debugger
exit status alone is insufficient. Apply a process timeout in unattended runners.
Sound and CD audio were disabled in rendering smoke tests; their hardware paths were not tested.

## OpenGL regression benchmarks

`build/vs2026/benchmark_opengl.py` compares the x86 and x64 Release builds using
original game demos, fresh matching configs, warmups, repeated alternating runs,
mode/driver validation, debugger guards, and a 30-second timeout per process.
It requires Windows SDK x86/x64 CDB and original PAKs in `x64/Release/baseq2`.

```bat
build\vs2026\build.bat Release Win32
build\vs2026\build.bat Release x64
python build/vs2026/benchmark_opengl.py --output x64/benchmarks/opengl --repeats 7
```

The stock Windows loop waits for a millisecond between frames, putting even
`timedemo 1` near a 1,000 FPS ceiling. Explicit `-benchmark` bypasses this wait only
while timedemo is active and uses the same fixed 16ms simulation step on both
architectures. The runner retains stock control runs and reports uncapped medians.
Normal gameplay timing is unchanged. The runner supports `--demo` and `--modes`.

On 2026-10-10, Ryzen 7 9700X / RTX 3080 / NVIDIA 616.92 system OpenGL, demo1's
seven-run x64 medians were 5.6-10.3% above x86 across 640x480 through 3840x2160.
Demo2's five-run cross-checks at 640x480, 1080p, 1440p, and 4K were also positive.
No major regression or captured error was observed. Individual short runs vary;
these measurements do not establish a guaranteed performance gain. Vsync and
audio were off. 4K used native fullscreen; lower resolutions were windowed.
Raw CSV, summaries, hashes, configs and logs are in the ignored `x64/benchmarks/`
output directories. Custom SGL drivers were not benchmarked in this sweep.

The following notes are historical documentation of the earlier Win32 migration.

---

# Quake 2 VC6 → VS2022 Migration Notes

## Overview

The original Quake 2 source release shipped with Microsoft Visual C++ 6.0 workspace/project files
(`.dsw` / `.dsp`). This document records the conversion to Visual Studio 2022 (MSBuild `.sln` /
`.vcxproj` format), assumptions made, and any source-level compatibility fixes applied.

---

## Original Project Layout

| Legacy File             | Project Name | Output Type | Output Filename          |
|-------------------------|-------------|-------------|--------------------------|
| `quake2.dsp`            | quake2      | Win32 EXE   | `quake2.exe`             |
| `game\game.dsp`         | game        | Win32 DLL   | `gamex86.dll`            |
| `ref_gl\ref_gl.dsp`     | ref_gl      | Win32 DLL   | `ref_gl.dll`             |
| `ref_soft\ref_soft.dsp` | ref_soft    | Win32 DLL   | `ref_soft.dll`           |
| `ctf\ctf.dsp`           | ctf         | Win32 DLL   | `gamex86.dll` (in ctf\)  |

All five projects were contained in `quake2.dsw`.

---

## Modern VS2022 Project Layout

Location: `build\vs2022\`

| Modern File             | Replaces                  | Output Directory          |
|-------------------------|---------------------------|---------------------------|
| `quake2.sln`            | `quake2.dsw`              | —                         |
| `quake2.vcxproj`        | `quake2.dsp`              | `Release\` / `Debug\`     |
| `game.vcxproj`          | `game\game.dsp`           | `Release\` / `Debug\`     |
| `ref_gl.vcxproj`        | `ref_gl\ref_gl.dsp`       | `Release\` / `Debug\`     |
| `ref_soft.vcxproj`      | `ref_soft\ref_soft.dsp`   | `Release\` / `Debug\`     |
| `ctf.vcxproj`           | `ctf\ctf.dsp`             | `ctf\Release\` / `ctf\Debug\` |

All source paths are referenced relative to the original source tree (no files were moved).
Intermediate object files go into `build\vs2022\obj\<Config>\<Project>\`.

---

## Configurations

The original `.dsp` files defined four configurations each:
- `Win32 Release` → mapped to **Release|Win32**
- `Win32 Debug` → mapped to **Debug|Win32**
- `Win32 Debug Alpha` → **dropped** (Alpha AXP CPU, not supported by any modern MSVC toolchain)
- `Win32 Release Alpha` → **dropped** (same reason)

---

## Compiler Flag Mapping (VC6 → VS2022)

| VC6 Flag | VS2022 Equivalent | Notes |
|----------|-------------------|-------|
| `/G5`    | *(removed)*       | Pentium-specific tuning; no equivalent in modern MSVC. The compiler auto-tunes. |
| `/GX`    | `/EHsc` (`<ExceptionHandling>Sync</ExceptionHandling>`) | Direct equivalent. |
| `/YX`    | *(removed)*       | Automatic PCH detection was removed in VS2005. No PCH used. |
| `/FR`    | *(removed)*       | Browse information; not supported in modern MSBuild. |
| `/Gm`    | *(removed)*       | Minimal rebuild was removed in VS2019. |
| `/Zd`    | `/Zi`             | Line-number-only debug format replaced by full PDB. |
| `/ZI`    | `EditAndContinue` | Still supported for x86 Debug. |
| `/O2`    | `O2`              | Direct equivalent. |
| `/Od`    | `Disabled`        | Direct equivalent. |
| `/MT`    | `MultiThreaded`   | Direct equivalent; static CRT. |
| `/MTd`   | `MultiThreadedDebug` | Direct equivalent. |
| `/W3`    | `Level3`          | Direct equivalent. |
| `/W4`    | `Level3`          | Downgraded to W3 to avoid excessive noise from 1997-era code. |

---

## Added Preprocessor Defines

The following defines were added globally (not present in original) to suppress modern MSVC
warnings that would be errors or extreme noise for 1997-era C code:

| Define | Purpose |
|--------|---------|
| `_CRT_SECURE_NO_WARNINGS` | Suppress deprecation warnings for `sprintf`, `strcpy`, `fopen`, etc. |
| `_WINSOCK_DEPRECATED_NO_WARNINGS` | Suppress deprecation warnings for old Winsock 1.1 API usage (quake2 project only). |

---

## Disabled Warnings (Per-Project)

These warnings are disabled narrowly via `<DisableSpecificWarnings>` because the original 1997 C
code triggers them pervasively and fixing them would require invasive changes:

| Warning | Reason |
|---------|--------|
| C4244 | `int`/`float` conversion (ubiquitous in Quake math) |
| C4305 | Truncation from `double` to `float` (all float literals treated as double in C) |
| C4018 | Signed/unsigned comparison |
| C4267 | `size_t` to `int` conversion |
| C4996 | Deprecated CRT functions (covered by `_CRT_SECURE_NO_WARNINGS` but listed for explicitness) |
| C4311 | Pointer truncation (32-bit pointer casts) |
| C4312 | Conversion to larger-size pointer |
| C4133 | Incompatible pointer types (old-style C function pointers) |
| C4028 | Formal parameter different from declaration |
| C4273 | Inconsistent DLL linkage (some headers vs. imports) |

---

## Assembly Files (ref_soft)

The software renderer (`ref_soft`) contains nine x86 MASM assembly files for performance-critical
inner loops:

```
r_aclipa.asm   r_draw16.asm   r_drawa.asm    r_edgea.asm
r_polysa.asm   r_scana.asm    r_spr8.asm     r_surf8.asm
r_varsa.asm
```

**Original build rule** (custom build step in VC6):
```
ml /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(InputPath)
```

**VS2022 implementation**: Uses the MASM build customization (`masm.props` / `masm.targets`),
with `/Cp /coff /Zm` passed as additional options. The `/coff` flag is required for Win32 COFF
object output. The `/Zm` flag enables MASM 5.1 compatibility mode for the 1997-era syntax.

**Known risk**: The assembly files use MASM syntax that may conflict with modern MASM defaults
(e.g., `ASSUME` directives, segment overrides). If assembly fails, exclude them and define
`C_ONLY` in preprocessor to fall back to the C implementations (as was done for Alpha builds).

---

## DirectDraw Dependency (ref_soft, quake2)

`win32\rw_ddraw.c` uses DirectDraw 7 (`ddraw.h`, `IDirectDraw7`). The files `ddraw.lib` and
`dxguid.lib` are included as linker dependencies. Both are available in the Windows 10 SDK
under `$(WindowsSdkDir)lib\$(WindowsSDKLibVersion)um\x86\`.

If linking fails with "cannot open file 'ddraw.lib'":
1. Open Visual Studio Installer
2. Ensure "Windows 10 SDK" is selected under the "Individual Components" tab
3. The lib is at: `C:\Program Files (x86)\Windows Kits\10\Lib\<version>\um\x86\ddraw.lib`

---

## OpenGL Loading (ref_gl)

The GL renderer (`ref_gl`) loads all OpenGL entry points dynamically at runtime via
`GetProcAddress` (implemented in `win32\qgl_win.c`). Therefore `opengl32.lib` is **not** listed
as a static link dependency, matching the original `.dsp`. If the dynamic loader fails at
runtime, add `opengl32.lib` to `ref_gl.vcxproj`'s `AdditionalDependencies`.

---

## Module Definition Files (.def)

All DLL projects use the original `.def` files for export control:

| Project  | DEF File              |
|----------|-----------------------|
| game     | `game\game.def`       |
| ref_gl   | `ref_gl\ref_gl.def`   |
| ref_soft | `ref_soft\ref_soft.def` |
| ctf      | `ctf\ctf.def`         |

These are referenced in each `.vcxproj` via `<ModuleDefinitionFile>` and were **not modified**.

---

## DLL Base Address

The original `game.dsp` specified `/base:"0x20000000"` for the game DLL. This is preserved in
`game.vcxproj` via `<BaseAddress>0x20000000</BaseAddress>`. This prevents relocation conflicts
when the engine loads both the game DLL and renderer DLLs simultaneously.

---

## Source Files Not Modified

The original source files are referenced as-is. No files were moved or renamed.

---

## Source-Level Fixes Applied

All fixes are minimal and clearly marked with `/* VS2022 compat: */` comments.

| File | Fix | Reason |
|------|-----|--------|
| `win32\winquake.rc:10` | `#include "afxres.h"` → `#include <winres.h>` | `afxres.h` is MFC-only; `winres.h` is the standard Windows SDK equivalent and provides all the same RC constants. |
| `win32\resource.h:5` | Added `#define IDD_DIALOG1 102` and `#define IDS_STRING1 103` | These were auto-generated by the VC6 resource editor but never saved in the released source. Required for the RC compiler to resolve the `IDD_DIALOG1` dialog and `IDS_STRING1` string table entry in `winquake.rc`. |

### Project File Corrections (no source changes)

| Issue | Fix |
|-------|-----|
| `win32/rw_ddraw.c`, `rw_dib.c`, `rw_imp.c`, `qgl_win.c` incorrectly attributed to main EXE | Removed from `quake2.vcxproj`; these belong only to `ref_soft.vcxproj` and `ref_gl.vcxproj`. The exploration of the original `.dsp` was imprecise; the actual `quake2.dsp` source list was verified against the file. |
| `<Optimization>O2</Optimization>` invalid in MSBuild | Changed to `<Optimization>MaxSpeed</Optimization>` (the MSBuild enum that maps to `/O2`). |
| `<DebugInformationFormat>EditAndContinue</DebugInformationFormat>` conflicted with `/INCREMENTAL:NO` | Changed to `ProgramDatabase` in all projects to eliminate LNK4075 warning. |
| Assembly objects in `ref_soft` lack SAFESEH data | Added `<ImageHasSafeExceptionHandlers>false</ImageHasSafeExceptionHandlers>` to the `ref_soft` linker settings (both Debug and Release). The 1997-era MASM code has no SEH handlers to register; this is correct and safe for a game DLL. |

---

## Files / Projects That Could Not Be Mapped

| Item | Reason |
|------|--------|
| Alpha (`Win32 Debug Alpha`, `Win32 Release Alpha`) | The DEC/Compaq Alpha AXP architecture is not supported by any shipping MSVC toolchain. Dropped. |
| `quake2.mak` | Legacy NMAKE makefile for VC6 command-line builds. Superseded by MSBuild. Not converted. |
| Linux/IRIX/Solaris/Rhapsody makefiles | Platform-specific; out of scope for Windows/VS2022 target. |
| `linux\*.s` assembly files | GAS syntax; cannot be assembled by MASM. Not included. |

---

## Build Prerequisites

- Visual Studio 2022 (v143 toolset) with "Desktop development with C++" workload
- Windows 10 SDK (any recent version, for `ddraw.h`, `ddraw.lib`, `dxguid.lib`)
- MASM (ml.exe for x86) — included with VS2022 C++ workload

---

## Building

```
# Open Developer Command Prompt for VS 2022
cd C:\Code\Quake-2\build\vs2022
msbuild quake2.sln /p:Configuration=Debug /p:Platform=Win32
msbuild quake2.sln /p:Configuration=Release /p:Platform=Win32
```

Or open `build\vs2022\quake2.sln` directly in Visual Studio 2022.

---

## Build Status (as of migration)

**All 5 projects build with 0 errors in both Debug|Win32 and Release|Win32.**

| Project    | Debug   | Release  | Output                        |
|------------|---------|----------|-------------------------------|
| quake2     | ✅ PASS | ✅ PASS  | `Debug\quake2.exe`            |
| game       | ✅ PASS | ✅ PASS  | `Debug\gamex86.dll`           |
| ref_gl     | ✅ PASS | ✅ PASS  | `Debug\ref_gl.dll`            |
| ref_soft   | ✅ PASS | ✅ PASS  | `Debug\ref_soft.dll`          |
| ctf        | ✅ PASS | ✅ PASS  | `ctf\Debug\gamex86.dll`       |

---

## Known Remaining Notes (no blockers)

1. **`x86.c`** — Contains inline x86 assembly (`__asm`). Win32 target only; fine as-is.
2. **`win32\rw_ddraw.c`** — DirectDraw 7; compiles and links cleanly. On modern Windows 11,
   DirectDraw is emulated via WARP and should function correctly at runtime.
3. **`wsock32.lib`** — Quake 2 uses Winsock 1.1. Still links and runs on modern Windows.
   A future cleanup could migrate to `ws2_32.lib`.
4. **Suppressed warnings** — C4244/4305/4018/4267/4133/4028 are disabled, not fixed.
   These pervasive type-mismatch warnings from 1997-era C would require invasive changes
   to correct; suppression is the correct conservative approach for this migration.
