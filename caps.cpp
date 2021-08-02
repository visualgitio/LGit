/*
 * Capabilities requested and provided for IDEs.
 */

#include "stdafx.h"
#include "LGit.h"

LONG LGitGetCaps(void)
{
	/* XXX: We could do IDE-specific hacks eventually. */
	LONG caps = /* all as of 1.3 listed */
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
		/* SccGetCommandOptions */
		| SCC_CAP_GETCOMMANDOPTIONS
		/* SccQueryInfo */
		| SCC_CAP_QUERYINFO
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
	LGitLog(" ! Caps %x\n", caps);
	return caps;
}

SCCEXTERNC SCCRTN EXTFUN __cdecl SccGetExtendedCapabilities (LPVOID pContext, 
															 LONG lSccExCap,
															 LPBOOL pbSupported)
{
	LGitLog("**SccGetExtendedCapabilities** Context=%p\n", pContext);
	switch (lSccExCap)
	{
	case SCC_EXCAP_CHECKOUT_LOCALVER:
		/* SCC_CHECKOUT_LOCALVER, whatever that does */
		LGitLog("  SCC_EXCAP_CHECKOUT_LOCALVER\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_BACKGROUND_GET:
		/* SccBackgroundGet (no need with git, also thread safety issues?) */
		LGitLog("  SCC_EXCAP_BACKGROUND_GET\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_ENUM_CHANGED_FILES:
		/* SccEnumChangedFiles */
		LGitLog("  SCC_EXCAP_ENUM_CHANGED_FILES\n");
		*pbSupported = TRUE;
		break;
	case SCC_EXCAP_POPULATELIST_DIR:
		/* SccPopulateDirList */
		LGitLog("  SCC_EXCAP_POPULATELIST_DIR\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_QUERYCHANGES:
		/* SccQueryChanges */
		LGitLog("  SCC_EXCAP_QUERYCHANGES\n");
		*pbSupported = TRUE;
		break;
	case SCC_EXCAP_ADD_FILES_FROM_SCC:
		/* SccAddFilesFromSCC */
		LGitLog("  SCC_EXCAP_ADD_FILES_FROM_SCC\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_GET_USER_OPTIONS:
		/* SccGetUserOption (just for localver) */
		LGitLog("  SCC_EXCAP_GET_USER_OPTIONS\n");
		*pbSupported = TRUE;
		break;
	case SCC_EXCAP_THREADSAFE_QUERY_INFO:
		/* If SccQueryInfo can be done from another thread. Unsure for lg2 */
		LGitLog("  SCC_EXCAP_THREADSAFE_QUERY_INFO\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_REMOVE_DIR:
		/* SccRemoveDir... documented in the header, undocumented in MSDN. */
		LGitLog("  SCC_EXCAP_REMOVE_DIR\n");
		*pbSupported = FALSE;
		break;
	case SCC_EXCAP_DELETE_CHECKEDOUT:
		/* If we can delete files that are checked out (sensical) */
		LGitLog("  SCC_EXCAP_DELETE_CHECKEDOUT\n");
		*pbSupported = TRUE;
		break;
	case SCC_EXCAP_RENAME_CHECKEDOUT:
		/* If we can rename files that are checked out (sensical) */
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
	LGitLog("**SccSetOption** Context=%p\n", context);
	switch (nOption) {
	case SCC_OPT_NAMECHANGEPFN:
		LGitLog("  SCC_OPT_NAMECHANGEPFN <- %p\n", dwVal);
		// XXX: How will this ever work on 64-bit?
		ctx->renameCb = (OPTNAMECHANGEPFN)dwVal;
		return SCC_OK;
	case SCC_OPT_USERDATA:
		LGitLog("  SCC_OPT_USERDATA <- %p\n", dwVal);
		ctx->renameData = (LPVOID)dwVal;
		return SCC_OK;
	case SCC_OPT_SHARESUBPROJ:
		LGitLog("  SCC_OPT_SHARESUBPROJ <- %x\n", dwVal);
		return SCC_I_SHARESUBPROJOK;
	case SCC_OPT_EVENTQUEUE:
		LGitLog("  SCC_OPT_EVENTQUEUE <- %x\n", dwVal);
		// Don't care
		return SCC_E_OPNOTSUPPORTED;
	case SCC_OPT_SCCCHECKOUTONLY:
		LGitLog("  SCC_OPT_SCCCHECKOUTONLY <- %x\n", dwVal);
		/* We don't offer "checkout" through RunScc */
		return SCC_E_OPNOTSUPPORTED;
	case SCC_OPT_HASCANCELMODE:
		LGitLog("  SCC_OPT_HASCANCELMODE <- %x\n", dwVal);
		/* We don't support cancellation */
		return SCC_E_OPNOTSUPPORTED;
	default:
		LGitLog("  %x <- %x\n", nOption, dwVal);
		return SCC_E_OPNOTSUPPORTED;
	}
}

SCCRTN SccGetUserOption(LPVOID context,
						LONG option,
						LONG val)
{
	LGitLog("**SccGetUserOption** Context=%p\n", context);
	switch (option) {
	case SCC_USEROPT_CHECKOUT_LOCALVER:
		LGitLog("  SCC_USEROPT_CHECKOUT_LOCALVER <- %x\n", val);
		return SCC_E_OPNOTSUPPORTED;
	default:
		LGitLog("  %x <- %x\n", option, val);
		return SCC_E_OPNOTSUPPORTED;
	}
}

SCCRTN SccIsMultiCheckoutEnabled (LPVOID pContext, 
								  LPBOOL pbMultiCheckout)
{
	LGitLog("**SccIsMultiCheckoutEnabled** Context=%p\n", pContext);
	*pbMultiCheckout = FALSE;
	return SCC_OK;
}

SCCRTN SccWillCreateSccFile (LPVOID pContext, 
							 LONG nFiles, 
							 LPCSTR* lpFileNames,
							 LPBOOL pbSccFiles)
{
	int i;
	LGitLog("**SccWillCreateSccFile** Context=%p\n", pContext);
	LGitLog("  files %d\n", nFiles);
	for (i = 0; i < nFiles; i++) {
		// Don't make SCC droppings
		pbSccFiles [i] = FALSE;
		LGitLog("  %s\n", lpFileNames[i]);
	}
	return SCC_OK;
}