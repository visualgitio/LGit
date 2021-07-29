/*
 * Initialization and sundry for the MSSCCI plugin.
 */

#include "stdafx.h"
#include "LGit.h"

/* This is initialized for dialog boxes later. */
static HINSTANCE dllInstance;

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			dllInstance = (HINSTANCE)hModule;
			break;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}

LONG SccGetVersion (void)
{
	LGitLog("**SccGetVersion** %x\n", SCC_VER_NUM);
	return SCC_VER_NUM;
}

static void LGitInitOpts(void)
{
	/* Options are global to the libgit2 singleton. */

	/* XXX: versions (us, lg2, Windows)? */
	git_libgit2_opts(GIT_OPT_SET_USER_AGENT,
		"VisualGit");
	/* Load options that are config-time from the registry. */
	HKEY key;
	DWORD valueLen, type = REG_SZ;
	BYTE value[255];
	int ret;

	/* Global system-wide, hence HKLM. We can load user settings later. */
	ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		"Software\\Visual Git\\Visual Git",
		0,
		KEY_READ,
		&key);
	if (ret != ERROR_SUCCESS) {
		LGitLog(" ! Opening the primary registry config failed\n");
		/* Try the old path */
		ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			"Software\\LGit\\LGit",
			0,
			KEY_READ,
			&key);
		if (ret != ERROR_SUCCESS) {
			LGitLog(" ! Opening the fallback registry config failed\n");
			return;
		}
	}

	/*
	 * OpenSSL setting, ignored for WinHTTP. This is because OpenSSL defaults
	 * to loading from a hardcoded path, which is ridiculous. Load from a path
	 * specified in the registry (by our installer ideally) instead.
	 */
	ret = RegQueryValueEx(key,
		"CertificateBundlePath",
		NULL,
		&type,
		value,
		&valueLen);
	if (ret == ERROR_SUCCESS) {
		/* Ignore return value since our transport may not use this */
		ret = git_libgit2_opts(GIT_OPT_SET_SSL_CERT_LOCATIONS,
			(const char*)value,
			NULL);
		if (ret != 0) {
			LGitLog(" ! Failed to set certificate path (may be harmless)\n");
		}
	} else {
		LGitLog(" ! Failed to load certificate bundle value\n");
	}

	RegCloseKey(key);
}

SCCRTN SccInitialize (LPVOID * context,				// SCC provider contex 
					  HWND hWnd,					// IDE window
					  LPCSTR callerName,			// IDE name
					  LPSTR sccName,				// SCC provider name
					  LPLONG sccCaps,				// SCC provider capabilities
					  LPSTR auxPathLabel,			// Aux path label, used in project open
					  LPLONG checkoutCommentLen,	// Check out comment max length
					  LPLONG commentLen)			// Other comments max length
{
	int init_count;

	init_count = git_libgit2_init();
	if (init_count < 0) {
		LGitLibraryError(hWnd, "Error initializing LGit");
		return SCC_E_INITIALIZEFAILED;
	}
	LGitLog("**SccInitialize** initialization count: %d (by %s)\n", init_count, callerName);

	/* Should this be only on the first init count? */
	LGitInitOpts();

	strlcpy(sccName, "Visual Git", SCC_NAME_LEN);

	*sccCaps = LGitGetCaps();

	/* XXX: What are the /real/ limits? */
	*checkoutCommentLen = 0; /* checkout comments are nonsensical */
	*commentLen = 1024;

	// XXX
	strlcpy (auxPathLabel, "LGitProject:", SCC_AUXLABEL_LEN);

	LGitLog("  LPVOID* Context=%p\n", context);
	LGitLog("         *Context=%p\n", *context);
	// It appears VS reuses
	if (context && *context == NULL) {
		*context = malloc(sizeof(LGitContext));
		LGitContext *ctx = (LGitContext*)*context;
		if (*context == NULL) {
			return SCC_E_INITIALIZEFAILED;
		}
		LGitLog("     New *Context=%p\n", *context);
		ZeroMemory(*context, sizeof(LGitContext));
		strlcpy(ctx->appName, callerName, SCC_NAME_LEN);
		ctx->dllInst = dllInstance;
		ctx->refcount = 1;
	} else if (context) {
		LGitContext *ctx = (LGitContext*)*context;
		LGitLog("     Recycling context\n");
		LGitLog("     ProjPath =%s\n", ctx->path);
		LGitLog("     WorkPath =%s\n", ctx->workdir_path);
		LGitLog("(Old)Refcount =%d\n", ctx->refcount);
		LGitLog("(New)Refcount =%d\n", ++ctx->refcount);
	}

	return SCC_OK;
}

SCCRTN SccUninitialize (LPVOID context)
{
	LGitContext *ctx = (LGitContext*)context;
	int uninit_count;

	uninit_count = git_libgit2_shutdown();
	LGitLog("**SccUninitialize** Context=%p\n", context);
	LGitLog("  Uninit count  =%d\n", uninit_count);
	if (uninit_count < 0) {
		LGitLibraryError(NULL, "Error ending LGit");
	}
	/*
	 * HACK: It seems Visual C++ 6, when switching workspaces, will call
	 * close, uninit... then open project, which frees this. Then it keeps
	 * the handle. Then devenv98 will keep multiple contexts around, so we
	 * should keep a reference count instead.
	 */
	if (context) {
		ctx->refcount--;
		LGitLog("  Refcount now %d\n", ctx->refcount);
	}
	if (context && ctx->refcount == 0) {
		LGitLog("  Freed context\n");
		free(context);
	}
	return SCC_OK;
}

SCCRTN SccRunScc(LPVOID context, 
				 HWND hWnd, 
				 LONG nFiles, 
				 LPCSTR* lpFileNames)
{
	int i;
	LGitLog("**SccRunScc** Context=%p\n", context);
	LGitLog("  files %d\n", nFiles);
	for (i = 0; i < nFiles; i++) {
		LGitLog("  %s\n", lpFileNames[i]);
	}
	MessageBox(hWnd, "Not implemented yet.", "Visual Git", MB_ICONWARNING);
	return SCC_E_OPNOTSUPPORTED;
}

SCCRTN SccGetCommandOptions (LPVOID context, 
							 HWND hWnd, 
							 enum SCCCOMMAND nCommand,
							 LPCMDOPTS * ppvOptions)
{
	LGitLog("**SccGetCommandOptions** Context=%p\n", context);
	LGitLog("  command %s\n", LGitCommandName(nCommand));
	return SCC_E_OPNOTSUPPORTED;
}

const char* LGitCommandName(enum SCCCOMMAND command)
{
	switch (command) {
	case -1:
		return "(No command)";
	case SCC_COMMAND_CHECKOUT:
		return "Checkout";
	case SCC_COMMAND_GET:
		return "Get";
	case SCC_COMMAND_CHECKIN:
		return "Checkin";
	case SCC_COMMAND_UNCHECKOUT:
		return "Uncheckout";
	case SCC_COMMAND_ADD:
		return "Add";
	case SCC_COMMAND_REMOVE:
		return "Remove";
	case SCC_COMMAND_DIFF:
		return "Diff";
	case SCC_COMMAND_HISTORY:
		return "History";
	case SCC_COMMAND_RENAME:
		return "Rename";
	case SCC_COMMAND_PROPERTIES:
		return "Properties";
	case SCC_COMMAND_OPTIONS:
		return "Options";
	default:
		/* should be able to return printed string, but I digress */
		return "Unknown";
	}
}