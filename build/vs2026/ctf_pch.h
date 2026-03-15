/*
 * ctf_pch.h - Precompiled header for the CTF mod DLL (ctf\gamex86.dll)
 *
 * CTF has its own copy of g_local.h in the ctf\ directory, which is listed
 * first in AdditionalIncludeDirectories (..\..\ctf before ..\..\game), so
 * "g_local.h" here resolves to ctf\g_local.h, not game\g_local.h.
 *
 * Resolved via AdditionalIncludeDirectories: ..\..\ctf (listed first)
 */
#pragma once
#include "g_local.h"
