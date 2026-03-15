/*
 * ref_gl_pch.h - Precompiled header for the OpenGL renderer DLL (ref_gl.dll)
 *
 * gl_local.h is the umbrella header for all ref_gl source files; it pulls in
 * q_shared.h, qfiles.h, ref.h, qgl.h, and the GL type definitions.
 * glw_imp.c and qgl_win.c (in win32/) also include gl_local.h, so force-
 * including it is safe across the whole project.
 *
 * Resolved via AdditionalIncludeDirectories: ..\..\ref_gl (listed first)
 */
#pragma once
#include "gl_local.h"
