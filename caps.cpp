/*
 * Capabilities requested and provided for IDEs.
 */

#include "stdafx.h"
#include "LGit.h"

LONG LGitGetCaps(void)
{
	/* XXX: We could do IDE-specific hacks eventually. */
	return /* all as of 1.3 listed */
		/* SccRemove */
		SCC_CAP_REMOVE
		/* SccRename */
		| SCC_CAP_RENAME 
		/* SccDiff */
		| SCC_CAP_DIFF
		/* SccHistory */
		| SCC_CAP_HISTORY
		/* SccProperties */
		| SCC_CAP_PROPERTIES
		/* SccRunScc */
		| SCC_CAP_RUNSCC
		/* SccGetCommandOptions; Not supported yet, will eventually */
		/* | SCC_GETCOMMANDOPTIONS */
		| SCC_CAP_QUERYINFO /* SccQueryInfo */
		/* SccGetEvents; Not support, may not make sense */
		/* | SCC_CAP_GETEVENTS */
		/* SccGetProjPath */
		| SCC_CAP_GETPROJPATH
		/* SccAddFromScc; "share" in IDE */
		| SCC_CAP_ADDFROMSCC
		/* Doesn't conceptually match to git. */
		/* | SCC_CAP_COMMENTCHECKOUT */
		/* Comment on commit */
		| SCC_CAP_COMMENTCHECKIN
		/* Comment on adding files to git */
		| SCC_CAP_COMMENTADD
		/* Comment on removing files from git */
		| SCC_CAP_COMMENTREMOVE
		/* Show status text through the IDE */
		| SCC_CAP_TEXTOUT
		/* SccCreateSubProject; unknown if mappable */
		/* | SCC_CAP_CREATESUBPROJECT */
		/* SccGetParentProject; unknown if mappable */
		/* | SCC_CAP_GETPARENTPROJECT */
		/* SccBeginBatch and SccEndBatch; unknown if useful */
		/* | SCC_CAP_BATCH */
		/* SccDirQueryInfo */
		| SCC_CAP_DIRECTORYSTATUS
		/* SccDirDiff */
		| SCC_CAP_DIRECTORYDIFF
		/* Store updated w/o deltas; can this map to git? Mode change? */
		/* | SCC_CAP_ADD_STORELATEST */
		/* Multiple files in SccHistory */
		| SCC_CAP_HISTORY_MULTFILE
		/* Case insensitive diffs; git doesn't support this? */
		/* | SCC_CAP_IGNORECASE */
		/* Whitespace ignoring diff; libgit2 option */
		| SCC_CAP_IGNORESPACE
		/* SccPopulateList */
		| SCC_CAP_POPULATELIST
		/* Comment on creating project; should map to initial commit? */
		| SCC_CAP_COMMENTPROJECT
		/* SccIsMultiCheckoutEnabled; maps to git? (but supports func) */
		| SCC_CAP_MULTICHECKOUT
		/* Always show diff even if IDE thinks there's no need */
		| SCC_CAP_DIFFALWAYS
		/* Disables the UI for SccGet; breaks "git clone" */
		/* | SCC_CAP_GET_NOUI */
		/* Threadsafe; IDEs mandate, libgit2 kinda supports it */
		| SCC_CAP_REENTRANT
		/* Creates MSSCCPRJ.SCC; we don't right now (but supports func) */
		| SCC_CAP_SCCFILE;
}

SCCEXTERNC SCCRTN EXTFUN __cdecl SccGetExtendedCapabilities (LPVOID pContext, 
															 LONG lSccExCap,
															 LPBOOL pbSupported)
{
	LGitLog("**SccGetExtendedCapabilities**\n");
	switch (lSccExCap)
	{
	case SCC_EXCAP_CHECKOUT_LOCALVER:
		LGitLog("  SCC_EXCAP_CHECKOUT_LOCALVER\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_BACKGROUND_GET:
		LGitLog("  SCC_EXCAP_BACKGROUND_GET\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_ENUM_CHANGED_FILES:
		LGitLog("  SCC_EXCAP_CHECKOUT_LOCALVER\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_POPULATELIST_DIR:
		LGitLog("  SCC_EXCAP_BACKGROUND_GET\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_QUERYCHANGES:
		LGitLog("  SCC_EXCAP_QUERYCHANGES\n");
		*pbSupported = TRUE;
		break;
	case SCC_EXCAP_ADD_FILES_FROM_SCC:
		LGitLog("  SCC_EXCAP_ADD_FILES_FROM_SCC\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_GET_USER_OPTIONS:
		LGitLog("  SCC_EXCAP_GET_USER_OPTIONS\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_THREADSAFE_QUERY_INFO:
		LGitLog("  SCC_EXCAP_THREADSAFE_QUERY_INFO\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_REMOVE_DIR:
		LGitLog("  SCC_EXCAP_REMOVE_DIR\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_DELETE_CHECKEDOUT:
		LGitLog("  SCC_EXCAP_DELETE_CHECKEDOUT\n");
		*pbSupported = TRUE;
		break;
	case SCC_EXCAP_RENAME_CHECKEDOUT:
		LGitLog("  SCC_EXCAP_RENAME_CHECKEDOUT\n");
		*pbSupported = TRUE;
		break;
	default:
		LGitLog("  ? %x\n", lSccExCap);
		*pbSupported = FALSE;
		break;
	}
	return SCC_OK;
}

SCCRTN SccSetOption (LPVOID context,
					 LONG nOption,
					 LONG dwVal)
{
	LGitContext *ctx = (LGitContext*)context;
	switch (nOption) {
	case SCC_OPT_NAMECHANGEPFN:
		LGitLog("**SccSetOption** SCC_OPT_NAMECHANGEPFN <- %p\n", dwVal);
		// XXX: How will this ever work on 64-bit?
		ctx->renameCb = (OPTNAMECHANGEPFN)dwVal;
		return SCC_OK;
	case SCC_OPT_USERDATA:
		LGitLog("**SccSetOption** SCC_OPT_USERDATA <- %p\n", dwVal);
		ctx->renameData = (LPVOID)dwVal;
		return SCC_OK;
	case SCC_OPT_SHARESUBPROJ:
		LGitLog("**SccSetOption** SCC_OPT_SHARESUBPROJ <- %x\n", dwVal);
		return SCC_I_SHARESUBPROJOK;
	case SCC_OPT_EVENTQUEUE:
		LGitLog("**SccSetOption** SCC_OPT_EVENTQUEUE <- %x\n", dwVal);
		// Don't care
		return SCC_E_OPNOTSUPPORTED;
	case SCC_OPT_SCCCHECKOUTONLY:
		LGitLog("**SccSetOption** SCC_OPT_SCCCHECKOUTONLY <- %x\n", dwVal);
		/* We don't offer "checkout" through RunScc */
		return SCC_E_OPNOTSUPPORTED;
	case SCC_OPT_HASCANCELMODE:
		LGitLog("**SccSetOption** SCC_OPT_HASCANCELMODE <- %x\n", dwVal);
		/* We don't support cancellation */
		return SCC_E_OPNOTSUPPORTED;
	default:
		LGitLog("**SccSetOption** %x <- %x\n", nOption, dwVal);
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