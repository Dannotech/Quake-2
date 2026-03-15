/*
 * ref_soft_pch.c - PCH creation translation unit for the ref_soft DLL project.
 *
 * Compiled with /Yc:r_local.h.  All r_*.c source files naturally include
 * r_local.h as their first include, triggering /Yu:r_local.h automatically.
 * The MASM assembly files (.asm) are compiled by ml.exe and are unaffected
 * by PrecompiledHeader settings (which only apply to ClCompile items).
 * q_shared.c, q_shwin.c, and the rw_*.c Win32 backend files have per-file
 * PrecompiledHeader=NotUsing overrides in the .vcxproj.
 */
#include "r_local.h"
