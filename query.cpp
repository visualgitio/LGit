/*
 * Query operations for letting the IDE know about git state
 */

#include "stdafx.h"
#include "LGit.h"

SCCRTN SccQueryInfo (LPVOID context, 
					 LONG nFiles, 
					 LPCSTR* lpFileNames, 
					 LPLONG lpStatus)
{

	LGitContext *ctx = (LGitContext*)context;
	int i, rc;
	unsigned int flags;
	LGitLog("**SccQueryInfo** count %d\n", nFiles);
	for (i = 0; i < nFiles; i++) {
		const char *raw_path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (raw_path == NULL) {
			LGitLog("    Error stripping %s\n", lpFileNames[i]);
			lpStatus[i] = SCC_STATUS_NOTCONTROLLED;
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		char path[1024];
		strncpy(path, raw_path, 1024);
		LGitTranslateStringChars(path, '\\', '/');
		rc = git_status_file(&flags, ctx->repo, path);
		LGitLog("    Adding %s, git status flags %x\n", path, flags);
		switch (rc) {
		case 0:
			break;
		case GIT_ENOTFOUND:
			lpStatus[i] = SCC_STATUS_NOTCONTROLLED;
			LGitLog("      Not found\n");
			continue;
		default:
			lpStatus[i] = SCC_STATUS_INVALID;
			LGitLibraryError(NULL, "Query info");
			LGitLog("      Error (%x)\n", rc);
			continue;
		}
		/*
		 * Files deleted from index by SccRemove will be GIT_STATUS_WT_NEW.
		 * I believe _WT_ corresponds to how it differs from the index.
		 * This should be an average file.
		 */
		if (!(flags & GIT_STATUS_WT_NEW)) {
			lpStatus[i] |= SCC_STATUS_CONTROLLED;
			/* We should return checked out; can't check in otherwise */
			lpStatus[i] |= SCC_STATUS_OUTMULTIPLE;
			lpStatus[i] |= SCC_STATUS_CHECKEDOUT;
		}
		/* Merge conflicts */
		if (flags & GIT_STATUS_CONFLICTED) {
			lpStatus[i] |= SCC_STATUS_MERGED;
		}
		/*
		 * If in index, but deleted from working tree.
		 * XXX: Checkout may actually be a suitable operation for this?
		 * XXX: I believe files just deleted by SccRemove would be WT_NEW,
		 *      which looks like any other deleted file...
		 */
		if (flags & GIT_STATUS_WT_DELETED) {
			lpStatus[i] |= SCC_STATUS_DELETED;
		}
		/* XXX: How do we map other things? Is what we have correct? */
		LGitLog("      Success, flags %x\n", lpStatus[i]);
		
	}
	return SCC_E_OPNOTSUPPORTED;
}

SCCRTN SccPopulateList (LPVOID context, 
						enum SCCCOMMAND nCommand, 
						LONG nFiles, 
						LPCSTR* lpFileNames, 
						POPLISTFUNC pfnPopulate, 
						LPVOID pvCallerData,
						LPLONG lpStatus, 
						LONG dwFlags)
{
	int i;
	LGitLog("**SccQueryInfo** count %d\n", nFiles);
	for (i = 0; i < nFiles; i++) {
		LGitLog("  %s\n", lpFileNames[i]);
	}
	return SCC_E_OPNOTSUPPORTED;
}

SCCRTN SccGetEvents (LPVOID context, 
					 LPSTR lpFileName,
					 LPLONG lpStatus,
					 LPLONG pnEventsRemaining)
{
	LGitLog("**SccGetEvents** %s\n", lpFileName);
	return SCC_E_OPNOTSUPPORTED;
}