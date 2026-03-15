/*
 * quake2_pch.h - Precompiled header for the main quake2 executable.
 *
 * The exe spans heterogeneous subsystems (client, server, common, win32) with
 * different umbrella headers (client.h, server.h, qcommon.h, winquake.h).
 * q_shared.h is the one header that is universally included by every
 * subsystem's include chain, making it the safe minimal PCH base.
 * It covers all core Quake 2 types (vec3_t, cvar_t, etc.) without pulling in
 * subsystem-specific definitions that could cause ordering conflicts.
 *
 * Resolved via AdditionalIncludeDirectories: ..\..\game
 */
#pragma once
#include "q_shared.h"
