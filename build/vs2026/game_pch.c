/*
 * game_pch.c - PCH creation translation unit for the game DLL project.
 *
 * Compiled with /Yc:g_local.h — MSVC saves compiler state after processing
 * g_local.h into the .pch binary.  All other game source files naturally
 * open with #include "g_local.h" which triggers /Yu:g_local.h automatically.
 * No ForcedIncludeFiles are needed, avoiding double-inclusion of the
 * guardless q_shared.h (which would cause C2365 redefinition errors).
 */
#include "g_local.h"
