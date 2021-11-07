/*
 * Initialization and sundry for the MSSCCI plugin.
 */

#include "stdafx.h"

/* This is initialized for dialog boxes later. */
static HINSTANCE dllInstance;

extern "C" BOOL APIENTRY DllMain(HANDLE hModule, DWORD reason, LPVOID _res)
{
    switch (reason)
	{
		case DLL_PROCESS_ATTACH:
			OutputDebugString("**Visual Git DllMain** proc attach\n");
			dllInstance = (HINSTANCE)hModule;
			break;
		case DLL_THREAD_ATTACH:
			OutputDebugString("**Visual Git DllMain** thread attach\n");
			break;
		case DLL_THREAD_DETACH:
			OutputDebugString("**Visual Git DllMain** thread deattach\n");
			break;
		case DLL_PROCESS_DETACH:
			OutputDebugString("**Visual Git DllMain** proc deattach\n");
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
	DWORD valueLen = 255, type = REG_SZ;
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

/* XXX: Call on DPI/theme changes */
static void InitImageLists(LGitContext *ctx)
{
	ctx->bitmapHeight = GetSystemMetrics(SM_CXSMICON);
	HIMAGELIST icons;
	HICON icon;
	if (ctx->refTypeIl != NULL) {
		ImageList_Destroy(ctx->refTypeIl);
	}
	icons = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		ILC_MASK, 6, 6);
	icon = LoadIcon(ctx->dllInst, MAKEINTRESOURCE(IDI_BRANCH));
	ImageList_AddIcon(icons, icon);
	DestroyIcon(icon);
	icon = LoadIcon(ctx->dllInst, MAKEINTRESOURCE(IDI_BRANCH_REMOTE));
	ImageList_AddIcon(icons, icon);
	DestroyIcon(icon);
	icon = LoadIcon(ctx->dllInst, MAKEINTRESOURCE(IDI_TAG));
	ImageList_AddIcon(icons, icon);
	DestroyIcon(icon);
	icon = LoadIcon(ctx->dllInst, MAKEINTRESOURCE(IDI_TAG_LIGHT));
	ImageList_AddIcon(icons, icon);
	DestroyIcon(icon);
	icon = LoadIcon(ctx->dllInst, MAKEINTRESOURCE(IDI_HEAD));
	ImageList_AddIcon(icons, icon);
	DestroyIcon(icon);
	ctx->refTypeIl = icons;
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
	LGitLog("**SccInitialize**\n");
	LGitLog("  Caller = %s\n", callerName);
	int init_count = git_libgit2_init();
	if (init_count < 0) {
		LGitLibraryError(hWnd, "Error initializing LGit");
		return SCC_E_INITIALIZEFAILED;
	}
	LGitLog("  InitCount = %d\n", init_count);

	/* Should this be only on the first init count? */
	LGitInitOpts();

	strlcpy(sccName, "Visual Git", SCC_NAME_LEN);

	*sccCaps = LGitGetCaps();

	/* XXX: What are the /real/ limits? */
	*checkoutCommentLen = 0; /* checkout comments are nonsensical */
	*commentLen = 2048;

	// XXX
	strlcpy (auxPathLabel, "LGitProject:", SCC_AUXLABEL_LEN);

	LGitLog("  LPVOID* Context=%p\n", context);
	LGitLog("         *Context=%p\n", *context);
	/*
	 * VC++ 6 will provide the last handle we initialized. Other IDEs don't do
	 * this. It's not some reuse scheme - don't get confused!
	 */
	if (context != NULL) {
		*context = malloc(sizeof(LGitContext));
		LGitContext *ctx = (LGitContext*)*context;
		if (*context == NULL) {
			return SCC_E_INITIALIZEFAILED;
		}
		LGitLog("     New *Context=%p\n", *context);
		ZeroMemory(*context, sizeof(LGitContext));
		strlcpy(ctx->appName, callerName, SCC_NAME_LEN);
		ctx->dllInst = dllInstance;

		/* gets reinitialized on project, but here, for empty proj */
		LGitInitializeFonts(ctx);

		/* initialize some stuff like image lists that can persist */
		InitImageLists(ctx);
	} else {
		return SCC_E_INITIALIZEFAILED;
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

	/* remember free shared resources */
	if (context == NULL) {
		LGitLog("!! Asked to free a null context\n");
	} else {
		ImageList_Destroy(ctx->refTypeIl);

		free(context);
		LGitLog("  Freed context\n");
	}
	return SCC_OK;
}