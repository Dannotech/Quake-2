/*
 * ref_soft_pch.h - Precompiled header for the software renderer DLL (ref_soft.dll)
 *
 * r_local.h is the umbrella header for all ref_soft C source files.
 * The MASM assembly files (.asm) are compiled by ml.exe, not cl.exe, so
 * ForcedIncludeFiles does not affect them - they remain untouched.
 *
 * Resolved via AdditionalIncludeDirectories: ..\..\ref_soft (listed first)
 */
#pragma once
#include "r_local.h"
