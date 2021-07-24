// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__FC742DCA_6BAE_4F0E_8EB5_A455A0655ABB__INCLUDED_)
#define AFX_STDAFX_H__FC742DCA_6BAE_4F0E_8EB5_A455A0655ABB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


// Insert your headers here
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wincrypt.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>

#if _DEBUG
#include <crtdbg.h>
#endif

// SCC (with compat stubs for VS6)
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
#include "scc_1_3.h"

// libgit2

#include <git2/global.h>
#include <git2/repository.h>
#include <git2/errors.h>
#include <git2/status.h>
#include <git2/checkout.h>
#include <git2/index.h>
#include <git2/signature.h>
#include <git2/commit.h>
#include <git2/revparse.h>
#include <git2/message.h>
#include <git2/buffer.h>
#include <git2/pathspec.h>
#include <git2/revwalk.h>
#include <git2/oid.h>
#include <git2/clone.h>
#include <git2/branch.h>

// our own stuff
#include "resource.h"

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__FC742DCA_6BAE_4F0E_8EB5_A455A0655ABB__INCLUDED_)
