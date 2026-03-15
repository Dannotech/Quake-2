/*
 * ctf_pch.c - PCH creation translation unit for the CTF DLL project.
 *
 * Compiled with /Yc:g_local.h (resolves to ctf\g_local.h via the include path
 * ordering: ..\..\ctf is listed before ..\..\game in AdditionalIncludeDirectories).
 * All CTF game source files naturally include g_local.h, triggering /Yu automatically.
 * ctf\q_shared.c has a per-file PrecompiledHeader=NotUsing override.
 */
#include "g_local.h"
