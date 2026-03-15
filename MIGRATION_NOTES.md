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
