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
	LGitLog("**SccInitializeInit** count: %d (by %s)\n", init_count, callerName);

	strcpy(sccName, "LGit");

	*sccCaps = SCC_CAP_REMOVE | /* SccRemove */
				SCC_CAP_RENAME | /* SccRename */
				SCC_CAP_DIFF | /* SccDiff */
				SCC_CAP_PROPERTIES | /* SccProperties */
				SCC_CAP_RUNSCC | /* SccRunScc */
				SCC_CAP_QUERYINFO | /* SccQueryInfo */
				SCC_CAP_COMMENTPROJECT | /* Comment on creating new project */
				SCC_CAP_COMMENTCHECKIN | /* ...comment on checkin */
				SCC_CAP_COMMENTREMOVE | /* ...comment on removing files */
				SCC_CAP_COMMENTADD | /* ...comment on adding files */
				//SCC_CAP_GET_NOUI | /* removes the UI for SccGet; no open from SCC opt if so */
				SCC_CAP_GETPROJPATH | /* SccGetProjPath *
				//SCC_CAP_TEXTOUT | /* Output through IDE */
				//SCC_CAP_MULTICHECKOUT | /* Multiple checkouts? */
				SCC_CAP_HISTORY | /* SccHistory */
				SCC_CAP_HISTORY_MULTFILE | /* multiple files with ^ */
				SCC_CAP_POPULATELIST | /* List files not known by IDE */
				//SCC_CAP_ADDFROMSCC | /* Seems to be share button? */
				//SCC_CAP_GETCOMMANDOPTIONS | /* Advanced button/addtl arg? */
				//SCC_CAP_ADD_STORELATEST | /* Storing without deltas? */
				SCC_CAP_REENTRANT; /* Thread-safe, VC++6 demands it */

	/* XXX: What are the /real/ limits? */
	*checkoutCommentLen = 1024;
	*commentLen = 1024;

	// XXX
	strcpy (auxPathLabel, "LGitProject:");

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
		strncpy(ctx->appName, callerName, SCC_NAME_LEN);
		ctx->dllInst = dllInstance;
	}

	return SCC_OK;
}

SCCRTN SccUninitialize (LPVOID context)
{
	LGitContext *ctx = (LGitContext*)context;
	int uninit_count;

	uninit_count = git_libgit2_shutdown();
	LGitLog("**SccUninitialize**\n");
	LGitLog("  Uninit count  =%d\n", uninit_count);
	LGitLog("  LPVOID Context=%p\n", context);
	if (uninit_count < 0) {
		LGitLibraryError(NULL, "Error ending LGit");
	}
	if (context) {
		free(context);
	}
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN __cdecl SccGetExtendedCapabilities (LPVOID pContext, 
															 LONG lSccExCap,
															 LPBOOL pbSupported)
{
	LGitLog("**SccGetExtendedCapabilities** %x\n", lSccExCap);
	switch (lSccExCap)
	{
	default:
		*pbSupported = FALSE;
	}
	return SCC_OK;
}

SCCRTN SccRunScc(LPVOID context, 
				 HWND hWnd, 
				 LONG nFiles, 
				 LPCSTR* lpFileNames)
{
	int i;
	LGitLog("**SccRunScc** count %d\n", nFiles);
	for (i = 0; i < nFiles; i++) {
		LGitLog("  %s\n", lpFileNames[i]);
	}
	MessageBox(hWnd, "Not implemented yet.", "LGit", MB_ICONWARNING);
	return SCC_E_OPNOTSUPPORTED;
}

SCCRTN SccGetCommandOptions (LPVOID context, 
							 HWND hWnd, 
							 enum SCCCOMMAND nCommand,
							 LPCMDOPTS * ppvOptions)
{
	LGitLog("**SccGetCommandOptions** Command %d\n", nCommand);
	return SCC_E_OPNOTSUPPORTED;
}

SCCRTN SccAddFromScc (LPVOID context, 
					  HWND hWnd, 
					  LPLONG pnFiles,
					  LPCSTR** lplpFileNames)
{
	LGitLog("**SccAddFromScc**\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCRTN SccSetOption (LPVOID context,
					 LONG nOption,
					 LONG dwVal)
{
	LGitLog("**SccSetOption** %x <- %x\n", nOption, dwVal);

	switch (nOption) {
	case SCC_OPT_NAMECHANGEPFN:
		// XXX: How will this ever work on 64-bit?
		((LGitContext*)context)->renameCb = (OPTNAMECHANGEPFN)dwVal;
		return SCC_OK;
	case SCC_OPT_EVENTQUEUE:
		// Don't care
		return SCC_E_OPNOTSUPPORTED;
	default:
		return SCC_E_OPNOTSUPPORTED;
	}
}

SCCRTN SccIsMultiCheckoutEnabled (LPVOID pContext, 
								  LPBOOL pbMultiCheckout)
{
	LGitLog("**SccIsMultiCheckoutEnabled**\n");
	*pbMultiCheckout = FALSE;
	return SCC_OK;
}

SCCRTN SccWillCreateSccFile (LPVOID pContext, 
							 LONG nFiles, 
							 LPCSTR* lpFileNames,
							 LPBOOL pbSccFiles)
{
	int i;
	LGitLog("**SccWillCreateSccFile** count %d\n", nFiles);
	for (i = 0; i < nFiles; i++) {
		// Don't make SCC droppings
		pbSccFiles [i] = FALSE;
		LGitLog("  %s\n", lpFileNames[i]);
	}
	return SCC_OK;
}