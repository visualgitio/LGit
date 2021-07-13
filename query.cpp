/*
 * Query operations for letting the IDE know about git state
 */

#include "stdafx.h"
#include "LGit.h"

static long LGitConvertFlags(unsigned int flags)
{
	/*
	 * Protip: changes relative from HEAD to stage/index are INDEX.
	 * Changes relative from stage/index to the working directory are WT.
	 */
	long sccFlags = 0;
	/* Files deleted from index by SccRemove will be GIT_STATUS_WT_NEW. */
	if (!(flags & GIT_STATUS_WT_NEW)) {
		sccFlags |= SCC_STATUS_CONTROLLED;
	}
	/*
	 * Only modified files will be considered checked out. At least IDEs
	 * only show diff/checkin/uncheckout then.
	 */
	if ((flags & GIT_STATUS_WT_MODIFIED)
		|| (flags & GIT_STATUS_WT_TYPECHANGE)
		|| (flags & GIT_STATUS_INDEX_MODIFIED)
		|| (flags & GIT_STATUS_INDEX_TYPECHANGE)) {
		sccFlags |= SCC_STATUS_OUTBYUSER;
		sccFlags |= SCC_STATUS_CHECKEDOUT;
	}
	/* Merge conflicts */
	if (flags & GIT_STATUS_CONFLICTED) {
		sccFlags |= SCC_STATUS_MERGED;
	}
	/* Files deleted by plain delete (rm) or index/stage delete (git rm) */
	if ((flags & GIT_STATUS_WT_DELETED)
		|| (flags & GIT_STATUS_INDEX_DELETED)) {
		sccFlags |= SCC_STATUS_DELETED;
	}
	/* XXX: How do we map other things? Is what we have correct? */
	return sccFlags;
}

SCCRTN SccQueryInfo (LPVOID context, 
					 LONG nFiles, 
					 LPCSTR* lpFileNames, 
					 LPLONG lpStatus)
{
	LGitContext *ctx = (LGitContext*)context;
	int i, rc;
	long sccFlags;
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
			sccFlags = LGitConvertFlags(flags);
			break;
		case GIT_ENOTFOUND:
			sccFlags = SCC_STATUS_NOTCONTROLLED;
			LGitLog("      Not found\n");
			break;
		default:
			sccFlags = SCC_STATUS_INVALID;
			LGitLibraryError(NULL, "Populate list");
			LGitLog("      Error (%x)\n", rc);
			break;
		}
		lpStatus[i] = sccFlags;
		if (rc != 0) {
			continue;
		}
		LGitLog("      Success, flags %x\n", lpStatus[i]);
	}
	return SCC_OK;
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
	LGitContext *ctx = (LGitContext*)context;
	int i, rc;
	long sccFlags;
	unsigned int flags;
	LGitLog("**SccPopulateList** command %x, flags %x count %d\n", nCommand, dwFlags, nFiles);
	/* First, look at the list and see what needs to be removed or added */
	for (i = 0; i < nFiles; i++) {
		if (dwFlags & SCC_PL_DIR) {
			LGitLog("    Not supported adding directory %s\n", lpFileNames[i]);
			lpStatus[i] = SCC_STATUS_INVALID;
			continue;
		}
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
			sccFlags = LGitConvertFlags(flags);
			break;
		case GIT_ENOTFOUND:
			sccFlags = SCC_STATUS_NOTCONTROLLED;
			LGitLog("      Not found\n");
			break;
		default:
			sccFlags = SCC_STATUS_INVALID;
			LGitLibraryError(NULL, "Populate list");
			LGitLog("      Error (%x)\n", rc);
			break;
		}
		lpStatus[i] = sccFlags;
		if (rc != 0) {
			continue;
		}
		LGitLog("      Success, flags %x\n", lpStatus[i]);
		/* Check what command */
		switch (nCommand) {
		case SCC_COMMAND_CHECKOUT:
			/* Nothing should be in the list here. */
			LGitLog(" ! Unpopulate for checkout\n");
			pfnPopulate(pvCallerData, FALSE, sccFlags, lpFileNames[i]);
			break;
		case SCC_COMMAND_GET:
		case SCC_COMMAND_CHECKIN:
		case SCC_COMMAND_UNCHECKOUT:
		case SCC_COMMAND_ADD:
		case SCC_COMMAND_REMOVE:
		/*
		 * case SCC_COMMAND_DIFF:
		 * case SCC_COMMAND_HISTORY:
		 * case SCC_COMMAND_RENAME:
		 * case SCC_COMMAND_PROPERTIES:
		 * case SCC_COMMAND_OPTIONS:
		 */
		default:
			/* Commented commands won't call populate, AFAIK */
			break;
		}
	}
	/* This is when we should list files the IDE didn't suggest */
	return dwFlags & SCC_PL_DIR ? SCC_E_OPNOTSUPPORTED : SCC_OK;
}

SCCRTN SccGetEvents (LPVOID context, 
					 LPSTR lpFileName,
					 LPLONG lpStatus,
					 LPLONG pnEventsRemaining)
{
	LGitLog("**SccGetEvents** %s\n", lpFileName);
	return SCC_E_OPNOTSUPPORTED;
}