/*
 * ref_gl_pch.c - PCH creation translation unit for the ref_gl DLL project.
 *
 * Compiled with /Yc:gl_local.h.  All gl_*.c source files naturally include
 * gl_local.h as their first include, triggering /Yu:gl_local.h automatically.
 * q_shared.c and q_shwin.c (which don't include gl_local.h) have per-file
 * PrecompiledHeader=NotUsing overrides in the .vcxproj.
 */
#include "gl_local.h"
