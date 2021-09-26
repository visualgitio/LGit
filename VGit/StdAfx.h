// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
#define AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#pragma comment(lib, "comctl32.lib")
#include <commctrl.h>

#include <stdio.h>

/* Keep in sync with ../LGit.h */
#define _Inout_opt_
#define _In_opt_z_
#define _In_
#define _In_z_
#define _Deref_out_
#define _Out_z_cap_(x)
#define _Out_
#define _Inout_
#define _Inout_z_
#define _In_count_(x)
#define _In_opt_
#define _Inout_cap_(x)
#define _Out_cap_(x)
#define _Deref_opt_out_opt_
#include "../scc_1_3.h"

#define LGIT_API extern "C" __declspec(dllimport)
LGIT_API SCCRTN LGitClone(void *ctx, HWND hWnd, LPSTR lpProjName, LPSTR lpLocalPath, LPBOOL pbNew);
LGIT_API SCCRTN LGitStandaloneExplorer(void *ctx, LONG nFiles, LPCSTR* lpFileNames);
LGIT_API void LGitTranslateStringChars(char *buf, int char1, int char2);
LGIT_API BOOL LGitGetProjectNameFromPath(char *project, const char *path, size_t bufsz);

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
