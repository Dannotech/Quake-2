/*
 * game_pch.h - Documentation header for the game DLL PCH strategy.
 *
 * NOTE: This file is NOT the PCH stop header. The stop header is g_local.h
 * itself (PrecompiledHeaderFile=g_local.h in game.vcxproj). game_pch.c
 * includes g_local.h directly via /Yc:g_local.h; all other game source files
 * trigger /Yu:g_local.h when they reach their natural #include "g_local.h".
 * No ForcedIncludeFiles are used — this avoids double-inclusion of q_shared.h
 * which has no include guards and would cause C2365 redefinition errors.
 */
#pragma once
#include "g_local.h"
