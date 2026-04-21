/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sys_win.h

#include "../qcommon/qcommon.h"
#include "../client/client.h"  // for client_static_t / connstate_t (ca_active etc.)
#include "winquake.h"
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include "../win32/conproc.h"

#define MINIMUM_WIN_MEMORY	0x0a00000
#define MAXIMUM_WIN_MEMORY	0x1000000

//#define DEMO

qboolean s_win95;

int			starttime;
int			ActiveApp;
qboolean	Minimized;

static HANDLE		hinput, houtput;

unsigned	sys_msg_time;
unsigned	sys_frame_time;

/* =========================================================================
   SGL regression-test harness hooks.

   -sgltest <mapname> <outputPath> [frames]

   When active, Q2 replaces its main loop with a deterministic frame stepper
   that loads the named map, runs a fixed number of frames (default 30) at
   a constant 16 ms/frame, writes a screenshot TGA to outputPath, then exits.
   Sources of wall-clock non-determinism (Sys_Milliseconds, wait counters,
   sound thread) are bypassed so runs produce byte-identical output.
   ========================================================================= */

// Defined in q_shwin.c so every module compiled against it (quake2.exe,
// ref_gl.dll, ref_soft.dll) links — only quake2.exe actually mutates them.
extern qboolean	sgltest_active;
extern int		sgltest_simulated_ms;

char		sgltest_mapname[64];
char		sgltest_outpath[MAX_OSPATH];
int			sgltest_frames = 30;
#define		SGLTEST_FIXED_DT_MS 16

/* =========================================================================
   VTune / timedemo profiling mode.

   -vtune [mapname=demo1.dm2]

   Normal Q2 main loop (so perf is realistic), but:
     * cvars pre-configured for SGL + windowed + timedemo
     * +map <mapname> injected so the demo starts immediately
     * After demo playback completes, auto-quits so VTune collection
       ends cleanly without a human clicking anything.
   Demo-end is detected by watching cls.state: after we've seen it reach
   ca_active at least once, the first transition back to ca_disconnected
   means the demo stream ended (before Q2's attract loop can chain to
   demo2).
   ========================================================================= */

qboolean	vtune_mode = false;
char		vtune_mapname[64] = "demo1.dm2";


static HANDLE		qwclsemaphore;

#define	MAX_NUM_ARGVS	128
int			argc;
char		*argv[MAX_NUM_ARGVS];


/*
===============================================================================

SYSTEM IO

===============================================================================
*/


void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	CL_Shutdown ();
	Qcommon_Shutdown ();

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	if (sgltest_active)
	{
		// No modal dialog in automated test runs — write to stderr and exit
		// non-zero so the harness script catches the failure.
		fprintf (stderr, "Q2 Sys_Error: %s\n", text);
		fflush (stderr);
	}
	else
	{
		MessageBox(NULL, text, "Error", 0 /* MB_OK */ );
	}

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);

// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (1);
}

void Sys_Quit (void)
{
	timeEndPeriod( 1 );

	CL_Shutdown();
	Qcommon_Shutdown ();
	CloseHandle (qwclsemaphore);
	if (dedicated && dedicated->value)
		FreeConsole ();

// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (0);
}


void WinError (void)
{
	LPVOID lpMsgBuf;

	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);

	// Display the string.
	MessageBox( NULL, lpMsgBuf, "GetLastError", MB_OK|MB_ICONINFORMATION );

	// Free the buffer.
	LocalFree( lpMsgBuf );
}

//================================================================


/*
================
Sys_ScanForCD

================
*/
char *Sys_ScanForCD (void)
{
	static char	cddir[MAX_OSPATH];
	static qboolean	done;
#ifndef DEMO
	char		drive[4];
	FILE		*f;
	char		test[MAX_QPATH];

	if (done)		// don't re-check
		return cddir;

	// no abort/retry/fail errors
	SetErrorMode (SEM_FAILCRITICALERRORS);

	drive[0] = 'c';
	drive[1] = ':';
	drive[2] = '\\';
	drive[3] = 0;

	done = true;

	// scan the drives
	for (drive[0] = 'c' ; drive[0] <= 'z' ; drive[0]++)
	{
		// where activision put the stuff...
		sprintf (cddir, "%sinstall\\data", drive);
		sprintf (test, "%sinstall\\data\\quake2.exe", drive);
		f = fopen(test, "r");
		if (f)
		{
			fclose (f);
			if (GetDriveType (drive) == DRIVE_CDROM)
				return cddir;
		}
	}
#endif

	cddir[0] = 0;
	
	return NULL;
}

/*
================
Sys_CopyProtect

================
*/
void	Sys_CopyProtect (void)
{
#ifndef DEMO
	char	*cddir;

	cddir = Sys_ScanForCD();
	if (!cddir[0])
		Com_Error (ERR_FATAL, "You must have the Quake2 CD in the drive to play.");
#endif
}


//================================================================


/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
	OSVERSIONINFO	vinfo;

#if 0
	// allocate a named semaphore on the client so the
	// front end can tell if it is alive

	// mutex will fail if semephore already exists
    qwclsemaphore = CreateMutex(
        NULL,         /* Security attributes */
        0,            /* owner       */
        "qwcl"); /* Semaphore name      */
	if (!qwclsemaphore)
		Sys_Error ("QWCL is already running on this system");
	CloseHandle (qwclsemaphore);

    qwclsemaphore = CreateSemaphore(
        NULL,         /* Security attributes */
        0,            /* Initial count       */
        1,            /* Maximum count       */
        "qwcl"); /* Semaphore name      */
#endif

	timeBeginPeriod( 1 );

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if (vinfo.dwMajorVersion < 4)
		Sys_Error ("Quake2 requires windows version 4 or greater");
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s)
		Sys_Error ("Quake2 doesn't run on Win32s");
	else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
		s_win95 = true;

	if (dedicated->value)
	{
		if (!AllocConsole ())
			Sys_Error ("Couldn't create dedicated server console");
		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);
	
		// let QHOST hook in
		InitConProc (argc, argv);
	}
}


static char	console_text[256];
static int	console_textlen;

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	INPUT_RECORD	recs[1024];
	int		dummy;
	int		ch, numread, numevents;

	if (!dedicated || !dedicated->value)
		return NULL;


	for ( ;; )
	{
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);	

						if (console_textlen)
						{
							console_text[console_textlen] = 0;
							console_textlen = 0;
							return console_text;
						}
						break;

					case '\b':
						if (console_textlen)
						{
							console_textlen--;
							WriteFile(houtput, "\b \b", 3, &dummy, NULL);	
						}
						break;

					default:
						if (ch >= ' ')
						{
							if (console_textlen < sizeof(console_text)-2)
							{
								WriteFile(houtput, &ch, 1, &dummy, NULL);	
								console_text[console_textlen] = ch;
								console_textlen++;
							}
						}

						break;

				}
			}
		}
	}

	return NULL;
}


/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput (char *string)
{
	int		dummy;
	char	text[256];

	if (!dedicated || !dedicated->value)
		return;

	if (console_textlen)
	{
		text[0] = '\r';
		memset(&text[1], ' ', console_textlen);
		text[console_textlen+1] = '\r';
		text[console_textlen+2] = 0;
		WriteFile(houtput, text, console_textlen+2, &dummy, NULL);
	}

	WriteFile(houtput, string, strlen(string), &dummy, NULL);

	if (console_textlen)
		WriteFile(houtput, console_text, console_textlen, &dummy, NULL);
}


/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
    MSG        msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage (&msg, NULL, 0, 0))
			Sys_Quit ();
		sys_msg_time = msg.time;
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	// grab frame time
	if (sgltest_active)
		sys_frame_time = (unsigned) sgltest_simulated_ms;  // deterministic clock
	else
		sys_frame_time = timeGetTime();	// FIXME: should this be at start?
}



/*
================
Sys_GetClipboardData

================
*/
char *Sys_GetClipboardData( void )
{
	char *data = NULL;
	char *cliptext;

	if ( OpenClipboard( NULL ) != 0 )
	{
		HANDLE hClipboardData;

		if ( ( hClipboardData = GetClipboardData( CF_TEXT ) ) != 0 )
		{
			if ( ( cliptext = GlobalLock( hClipboardData ) ) != 0 ) 
			{
				data = malloc( GlobalSize( hClipboardData ) + 1 );
				strcpy( data, cliptext );
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
	return data;
}

/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate (void)
{
	ShowWindow ( cl_hwnd, SW_RESTORE);
	SetForegroundWindow ( cl_hwnd );
}

/*
========================================================================

GAME DLL

========================================================================
*/

static HINSTANCE	game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (!FreeLibrary (game_library))
		Com_Error (ERR_FATAL, "FreeLibrary failed for game library");
	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms)
{
	void	*(*GetGameAPI) (void *);
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];
#if defined _M_IX86
	const char *gamename = "gamex86.dll";

#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif

#elif defined _M_ALPHA
	const char *gamename = "gameaxp.dll";

#ifdef NDEBUG
	const char *debugdir = "releaseaxp";
#else
	const char *debugdir = "debugaxp";
#endif

#endif

	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	// check the current debug directory first for development purposes
	_getcwd (cwd, sizeof(cwd));
	Com_sprintf (name, sizeof(name), "%s/%s/%s", cwd, debugdir, gamename);
	game_library = LoadLibrary ( name );
	if (game_library)
	{
		Com_DPrintf ("LoadLibrary (%s)\n", name);
	}
	else
	{
		// check the current directory for other development purposes
		Com_sprintf (name, sizeof(name), "%s/%s", cwd, gamename);
		game_library = LoadLibrary ( name );
		if (game_library)
		{
			Com_DPrintf ("LoadLibrary (%s)\n", name);
		}
		else
		{
			// now run through the search paths
			path = NULL;
			while (1)
			{
				path = FS_NextPath (path);
				if (!path)
					return NULL;		// couldn't find one anywhere
				Com_sprintf (name, sizeof(name), "%s/%s", path, gamename);
				game_library = LoadLibrary (name);
				if (game_library)
				{
					Com_DPrintf ("LoadLibrary (%s)\n",name);
					break;
				}
			}
		}
	}

	GetGameAPI = (void *)GetProcAddress (game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame ();		
		return NULL;
	}

	return GetGameAPI (parms);
}

//=======================================================================


/*
==================
ParseCommandLine

==================
*/
void ParseCommandLine (LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
			
		}
	}

}

/*
==================
SglTest_ParseArgs

Scans argv[] for "-sgltest <mapname> <outputPath> [frames]", extracts the
parameters into the sgltest_* globals, and rewrites the remaining argv so
Q2's normal arg parser doesn't see our switch. Also synthesizes a batch of
"+set" overrides that pin every source of non-determinism before the
renderer initializes. Returns true if sgltest mode was requested.
==================
*/
static qboolean SglTest_ParseArgs (void)
{
	int i, j, consumed;

	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-sgltest") != 0)
			continue;

		if (i + 2 >= argc)
		{
			fprintf (stderr, "-sgltest requires <mapname> <outputPath> [frames]\n");
			exit (2);
		}

		strncpy (sgltest_mapname, argv[i+1], sizeof(sgltest_mapname) - 1);
		sgltest_mapname[sizeof(sgltest_mapname) - 1] = '\0';
		strncpy (sgltest_outpath, argv[i+2], sizeof(sgltest_outpath) - 1);
		sgltest_outpath[sizeof(sgltest_outpath) - 1] = '\0';

		consumed = 3;
		if (i + 3 < argc && argv[i+3][0] >= '0' && argv[i+3][0] <= '9')
		{
			sgltest_frames = atoi (argv[i+3]);
			if (sgltest_frames < 1) sgltest_frames = 1;
			consumed = 4;
		}

		// Remove "-sgltest ..." from argv so the main parser ignores it.
		for (j = i; j + consumed < argc; j++)
			argv[j] = argv[j + consumed];
		argc -= consumed;

		sgltest_active = true;

		// Synthesize "+set" overrides. These land in argv for Qcommon_Init's
		// Cbuf_AddEarlyCommands, applied AFTER config.cfg so they win. Each
		// triple must fit; cap at MAX_NUM_ARGVS.
		{
			static const char *overrides[][3] = {
				{ "vid_ref",        "gl"                   },   // Use ref_gl (GL path), not ref_soft.
				{ "gl_driver",      "SGL.dll"              },   // Force SGL as the GL driver, not opengl32.
				{ "vid_fullscreen", "0"                    },   // Windowed, fixed size.
				{ "gl_mode",        "3"                    },   // 640x480, deterministic.
				{ "s_initsound",    "0"                    },   // No audio thread.
				{ "cl_introPlayed", "1"                    },   // Skip intro cinematic.
				{ "vid_xpos",       "0"                    },   // Fixed window position.
				{ "vid_ypos",       "0"                    },
				{ "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST" }, // Pin texture filter. gl_texturemode is
				// CVAR_ARCHIVE, so config.cfg can contaminate across runs unless we pin. Matches Q2's
				// out-of-box default so goldens were captured with this value too.
			};
			int k;
			int n = (int)(sizeof(overrides) / sizeof(overrides[0]));
			for (k = 0; k < n && argc + 3 <= MAX_NUM_ARGVS; k++)
			{
				argv[argc++] = "+set";
				argv[argc++] = (char *)overrides[k][0];
				argv[argc++] = (char *)overrides[k][1];
			}
		}

		return true;
	}
	return false;
}

/*
==================
VTune_ParseArgs

Scans argv[] for "-vtune [mapname]", extracts the optional map name,
rewrites argv, synthesizes the +set overrides and a +map command so that
the normal Q2 main loop enters timedemo playback on the named demo.
Default map is demo1.dm2 (ships in stock baseq2/pak0).
==================
*/
static qboolean VTune_ParseArgs (void)
{
	int i, j, consumed;

	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-vtune") != 0)
			continue;

		consumed = 1;
		// Optional inline map arg. Reject things that look like another
		// switch (+foo, -foo) so "-vtune +set x y" still works.
		if (i + 1 < argc && argv[i+1][0] != '+' && argv[i+1][0] != '-')
		{
			strncpy (vtune_mapname, argv[i+1], sizeof(vtune_mapname) - 1);
			vtune_mapname[sizeof(vtune_mapname) - 1] = '\0';
			consumed = 2;
		}

		// Remove our switch from argv.
		for (j = i; j + consumed < argc; j++)
			argv[j] = argv[j + consumed];
		argc -= consumed;

		vtune_mode = true;

		{
			static const char *overrides[][3] = {
				{ "vid_ref",        "gl"                   },   // Use ref_gl, not ref_soft.
				{ "gl_driver",      "SGL.dll"              },   // Force SGL.
				{ "vid_fullscreen", "0"                    },   // Windowed so VTune can sample without lockups.
				{ "gl_mode",        "3"                    },   // 640x480 -- consistent surface area.
				{ "cl_timedemo",    "1"                    },   // Uncap FPS; print timing on demo end.
				{ "cl_introPlayed", "1"                    },   // Skip intro cinematic.
				{ "s_initsound",    "0"                    },   // No audio thread noise in the profile.
				{ "vid_xpos",       "0"                    },
				{ "vid_ypos",       "0"                    },
				{ "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST" },
			};
			int k, n = (int)(sizeof(overrides) / sizeof(overrides[0]));
			for (k = 0; k < n && argc + 3 <= MAX_NUM_ARGVS; k++)
			{
				argv[argc++] = "+set";
				argv[argc++] = (char *)overrides[k][0];
				argv[argc++] = (char *)overrides[k][1];
			}
			// Kick off the demo immediately (late command, runs after renderer init).
			if (argc + 2 <= MAX_NUM_ARGVS)
			{
				argv[argc++] = "+map";
				argv[argc++] = vtune_mapname;
			}
		}

		return true;
	}
	return false;
}

/*
==================
SglTest_Run

Replaces Q2's main message loop when -sgltest is active. Drives exactly
sgltest_frames frames with a constant SGLTEST_FIXED_DT_MS timestep, writes
the screenshot, and quits. Never reads Sys_Milliseconds in the render path.
==================
*/
static void SglTest_Run (void)
{
	MSG		msg;
	int		i;

	/* Defensive checks. Q2 silently falls back to the SW renderer if vid_ref
	   isn't "gl", or to opengl32 if gl_driver's DLL didn't load. Either
	   produces a screenshot that has nothing to do with SGL — a false pass.
	   Fail fast with distinct exit codes so the harness script can tell
	   them apart in the log. */
	{
		cvar_t *vid_ref_cv = Cvar_Get ("vid_ref", "soft", CVAR_ARCHIVE);
		if (strcmp (vid_ref_cv->string, "gl") != 0) {
			fprintf (stderr,
				"-sgltest: vid_ref is \"%s\", not \"gl\". "
				"Q2 is using the software renderer so SGL is not being driven.\n",
				vid_ref_cv->string);
			fflush (stderr);
			exit (4);
		}
	}

	if (!GetModuleHandleA ("SGL.dll"))
	{
		fprintf (stderr,
			"-sgltest: SGL.dll is not loaded in this process. "
			"Q2 fell back to the default GL driver. Aborting to avoid a false pass.\n");
		fflush (stderr);
		exit (3);
	}

	_controlfp (_PC_24, _MCW_PC);

	// Load the test map synchronously.
	Cbuf_AddText (va("map %s\n", sgltest_mapname));
	Cbuf_Execute ();

	// Re-seed rand() after map load. Map loading consumes some number of
	// rand() calls (entity spawning, pak indexing, etc) that we don't have
	// tight control over; re-seeding here guarantees the gameplay-frame
	// particle/fx RNG sequence is deterministic from a known state.
	srand (42);

	// Step a fixed number of frames at a fixed dt — nothing reads wall-clock here.
	// sgltest_simulated_ms is the virtual clock Sys_Milliseconds returns while
	// the test is active; advancing it here keeps cls.realtime and any timeout
	// arithmetic in lockstep with our injected frame dt.
	for (i = 0; i < sgltest_frames; i++)
	{
		// Drain pending window messages so the window stays responsive and
		// WM_PAINT etc don't pile up, but don't let them drive timing.
		while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage (&msg, NULL, 0, 0))
			{
				Com_Quit ();
				return;
			}
			sys_msg_time = msg.time;
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}

		sgltest_simulated_ms += SGLTEST_FIXED_DT_MS;
		Qcommon_Frame (SGLTEST_FIXED_DT_MS);
	}

	// Fire the screenshot via our test-only command so ref_gl writes to the
	// absolute path we were given instead of baseq2/scrnshot/quakeNN.tga.
	Cbuf_AddText (va("sgltest_screenshot \"%s\"\n", sgltest_outpath));
	Cbuf_Execute ();

	Sys_Quit ();
}

/*
==================
WinMain

==================
*/
HINSTANCE	global_hInstance;

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG				msg;
	int				time, oldtime, newtime;
	char			*cddir;

    /* previous instances do not exist in Win32 */
    if (hPrevInstance)
        return 0;

	global_hInstance = hInstance;

	ParseCommandLine (lpCmdLine);

	// Intercept -sgltest / -vtune before Sys_ScanForCD et al — they synthesize
	// their own argv additions and select alternate startup paths.
	SglTest_ParseArgs ();
	VTune_ParseArgs ();

	// if we find the CD, add a +set cddir xxx command line
	cddir = Sys_ScanForCD ();
	if (cddir && argc < MAX_NUM_ARGVS - 3)
	{
		int		i;

		// don't override a cddir on the command line
		for (i=0 ; i<argc ; i++)
			if (!strcmp(argv[i], "cddir"))
				break;
		if (i == argc)
		{
			argv[argc++] = "+set";
			argv[argc++] = "cddir";
			argv[argc++] = cddir;
		}
	}

	Qcommon_Init (argc, argv);

	if (sgltest_active)
	{
		SglTest_Run ();
		return 0;  // never reached — SglTest_Run calls Sys_Quit.
	}

	oldtime = Sys_Milliseconds ();

    /* main window message loop */
	while (1)
	{
		// if at a full screen console, don't update unless needed
		if (Minimized || (dedicated && dedicated->value) )
		{
			Sleep (1);
		}

		while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage (&msg, NULL, 0, 0))
				Com_Quit ();
			sys_msg_time = msg.time;
			TranslateMessage (&msg);
   			DispatchMessage (&msg);
		}

		do
		{
			newtime = Sys_Milliseconds ();
			time = newtime - oldtime;
		} while (time < 1);
//			Con_Printf ("time:%5.2f - %5.2f = %5.2f\n", newtime, oldtime, time);

		//	_controlfp( ~( _EM_ZERODIVIDE /*| _EM_INVALID*/ ), _MCW_EM );
		_controlfp( _PC_24, _MCW_PC );
		Qcommon_Frame (time);

		if (vtune_mode)
		{
			extern client_static_t cls;
			static qboolean demo_started = false;
			static int      active_frames = 0;
			if (cls.state == ca_active)
			{
				if (++active_frames >= 5)
					demo_started = true;
			}
			else if (demo_started && cls.state == ca_disconnected)
			{
				// Demo stream ended. Quit before the attract loop chains
				// into demo2. cl_timedemo has already printed its fps
				// summary by this point.
				Com_Printf ("[vtune] demo complete, exiting\n");
				Com_Quit ();
			}
		}

		oldtime = newtime;
	}

	// never gets here
    return TRUE;
}
