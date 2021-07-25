/*
 * Query operations for letting the IDE know about git state
 */

#include "stdafx.h"
#include "LGit.h"

static long LGitConvertFlags(LGitContext *ctx,
							 const char *fileName,
							 unsigned int flags)
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
	/* Append fake checkout marker, since VB6 and VS.NET want to see them */
	if (LGitIsCheckout(ctx, fileName)) {
		LGitLog(" ! Fake checkout flag for %s\n", fileName);
		sccFlags |= SCC_STATUS_OUTBYUSER;
		sccFlags |= SCC_STATUS_CHECKEDOUT;
		/* Little marker so we now it's not really modified */
		sccFlags |= SCC_STATUS_OUTEXCLUSIVE;
	}
	/* XXX: How do we map other things? Is what we have correct? */
	return sccFlags;
}

static void LGitCallPopulateAction(enum SCCCOMMAND nCommand,
								   LPCSTR fileName,
								   unsigned int lg2flags,
								   long sccFlags,
								   POPLISTFUNC pfnPopulate,
								   LPVOID pvCallerData)
{
	BOOL allow = FALSE;
	/* Check what command */
	switch (nCommand) {
	case SCC_COMMAND_CHECKOUT:
		/* Since we fake checkout, we're obligated. Before, always not. */
		if (!(sccFlags & SCC_STATUS_CHECKEDOUT)) {
			allow = TRUE;
		}
		pfnPopulate(pvCallerData, allow, sccFlags, fileName);
		break;
	case SCC_COMMAND_GET:
		/* Everything should be in the list here, it's called by clone */
		pfnPopulate(pvCallerData, TRUE, sccFlags, fileName);
		break;
	case SCC_COMMAND_CHECKIN:
		/* Use same comparison as Convert, so we can ignore fake checkouts */
		if ((lg2flags & GIT_STATUS_WT_MODIFIED)
			|| (lg2flags & GIT_STATUS_WT_TYPECHANGE)
			|| (lg2flags & GIT_STATUS_INDEX_MODIFIED)
			|| (lg2flags & GIT_STATUS_INDEX_TYPECHANGE)) {
			allow = TRUE;
		}
		pfnPopulate(pvCallerData, allow, sccFlags, fileName);
		break;
	case SCC_COMMAND_UNCHECKOUT:
		if (sccFlags & SCC_STATUS_CHECKEDOUT) {
			allow = TRUE;
		}
		pfnPopulate(pvCallerData, allow, sccFlags, fileName);
		break;
	case SCC_COMMAND_ADD:
		if (!(sccFlags & SCC_STATUS_CONTROLLED)) {
			allow = TRUE;
		}
		pfnPopulate(pvCallerData, allow, sccFlags, fileName);
		break;
	case SCC_COMMAND_REMOVE:
		if (sccFlags & SCC_STATUS_CONTROLLED) {
			allow = TRUE;
		}
		pfnPopulate(pvCallerData, allow, sccFlags, fileName);
		break;
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

typedef struct _LGitStatusCallbackParams {
	LGitContext *ctx;
	/* To Microsoft functions */
	enum SCCCOMMAND nCommand;
	POPLISTFUNC pfnPopulate;
	LPVOID pvCallerData;
} LGitStatusCallbackParams;

static int LGitStatusCallback(const char *relative_path,
							  unsigned int flags,
							  void *context)
{
	LGitStatusCallbackParams *params = (LGitStatusCallbackParams*)context;
	long sccFlags = LGitConvertFlags(params->ctx, relative_path, flags);
	LGitLog(" ! Entry \"%s\" (%x -> %x)\n", relative_path, flags, sccFlags);
	/* Merge for absolute path */
	char path[2048];
	strlcpy(path, params->ctx->workdir_path, 2048);
	strlcat(path, relative_path, 2048);
	LGitTranslateStringChars(path, '/', '\\');

	LGitLog(" ! Pushing absolute path \"%s\"", path);
	LGitCallPopulateAction(params->nCommand,
		path,
		flags,
		sccFlags,
		params->pfnPopulate,
		params->pvCallerData);
	return 0;
}

static SCCRTN LGitPopulateDirs (LPVOID context,
								enum SCCCOMMAND nCommand, 
								LONG nFiles, 
								LPCSTR* lpFileNames, 
								POPLISTFUNC pfnPopulate, 
								LPVOID pvCallerData)
{
	LGitContext *ctx = (LGitContext*)context;
	char **paths = NULL;

	git_status_options sopts;
	LGitStatusCallbackParams cbp;

	git_status_options_init(&sopts, GIT_STATUS_OPTIONS_VERSION);

	/* If there's filenames, generate a pathspec, otherwise all files. */
	if (nFiles > 0) {
		LGitLog(" ! Populating directories\n");
		paths = (char**)calloc(sizeof(char*), nFiles);
		if (paths == NULL) {
			return SCC_E_NONSPECIFICERROR;
		}
		int i, path_count;
		const char *raw_path;
		path_count = 0;
		for (i = 0; i < nFiles; i++) {
			char *path;
			raw_path = LGitStripBasePath(ctx, lpFileNames[i]);
			if (raw_path == NULL) {
				LGitLog("    Couldn't get base path for dir %s\n", lpFileNames[i]);
				continue;
			}
			/* Translate because libgit2 operates with forward slashes */
			path = strdup(raw_path);
			LGitTranslateStringChars(path, '\\', '/');
			LGitLog("    Dir %s\n", path);
			paths[path_count++] = path;
		}
		sopts.pathspec.strings = paths;
		sopts.pathspec.count = path_count;
	} else {
		LGitLog(" ! Populating repo root\n");
	}

	/*
	 * Arguably, status isn't the best API for enumerating everything, but it
	 * provides the all-important status. Just widen the net we cast. It's
	 * mostly important to clone, so it can determine if there's any project
	 * files to look for.
	 */
	sopts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	sopts.flags = GIT_STATUS_OPT_DEFAULTS
		| GIT_STATUS_OPT_INCLUDE_UNREADABLE
		| GIT_STATUS_OPT_INCLUDE_UNMODIFIED;

	cbp.ctx = ctx;
	cbp.nCommand = nCommand;
	cbp.pfnPopulate = pfnPopulate;
	cbp.pvCallerData = pvCallerData;
	git_status_foreach_ext(ctx->repo, &sopts, LGitStatusCallback, &cbp);
	LGitLog(" ! Done enumerating\n");

	if (paths != NULL) {
		free(paths);
	}
	return SCC_OK;
}

static SCCRTN LGitPopulateFiles(LPVOID context,
								enum SCCCOMMAND nCommand, 
								LONG nFiles, 
								LPCSTR* lpFileNames, 
								POPLISTFUNC pfnPopulate, 
								LPVOID pvCallerData,
								LPLONG lpStatus)
{
	LGitContext *ctx = (LGitContext*)context;
	int i, rc;
	long sccFlags;
	unsigned int flags;
	LGitLog(" ! Populating files\n");
	for (i = 0; i < nFiles; i++) {
		const char *raw_path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (raw_path == NULL) {
			LGitLog("    Error stripping %s\n", lpFileNames[i]);
			lpStatus[i] = SCC_STATUS_NOTCONTROLLED;
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		char path[1024];
		strlcpy(path, raw_path, 1024);
		LGitTranslateStringChars(path, '\\', '/');
		rc = git_status_file(&flags, ctx->repo, path);
		LGitLog("    Adding %s, git status flags %x\n", path, flags);
		switch (rc) {
		case 0:
			sccFlags = LGitConvertFlags(ctx, path, flags);
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
		if (pfnPopulate != NULL) {
			LGitCallPopulateAction(nCommand, lpFileNames[i], flags, sccFlags, pfnPopulate, pvCallerData);
		}
	}
	return SCC_OK;
}

/**
 * SccQueryInfo is purely a subset of PopulateList, so...
 */
static SCCRTN LGitPopulateList(LPVOID context, 
						enum SCCCOMMAND nCommand, 
						LONG nFiles, 
						LPCSTR* lpFileNames, 
						POPLISTFUNC pfnPopulate, 
						LPVOID pvCallerData,
						LPLONG lpStatus, 
						LONG dwFlags)
{
	if (dwFlags & SCC_PL_DIR) {
		/* Seems lpStatus is ignored with directories, as we push instead */
		return LGitPopulateDirs(context, nCommand, nFiles, lpFileNames, pfnPopulate, pvCallerData);
	} else {
		return LGitPopulateFiles(context, nCommand, nFiles, lpFileNames, pfnPopulate, pvCallerData, lpStatus);
	}
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
	LGitLog("**SccPopulateList** command %s, flags %x count %d\n", LGitCommandName(nCommand), dwFlags, nFiles);
	return LGitPopulateList(context, nCommand, nFiles, lpFileNames, pfnPopulate, pvCallerData, lpStatus, dwFlags);
}

SCCRTN SccQueryInfo (LPVOID context, 
					 LONG nFiles, 
					 LPCSTR* lpFileNames, 
					 LPLONG lpStatus)
{
	LGitLog("**SccQueryInfo** count %d\n", nFiles);
	return LGitPopulateList(context, (enum SCCCOMMAND)-1, nFiles, lpFileNames, NULL, NULL, lpStatus, 0);
}

SCCRTN SccDirQueryInfo(LPVOID context,
					   LONG nDirs,
					   LPCSTR* lpDirNames,
					   LPLONG lpStatus)
{
	LGitContext *ctx = (LGitContext*)context;
	int i, rc;
	unsigned int flags;
	const char *raw_path;
	LGitLog("**SccDirQueryInfo** count %d\n", nDirs);
	for (i = 0; i < nDirs; i++) {
		char *path;
		raw_path = LGitStripBasePath(ctx, lpDirNames[i]);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for dir %s\n", lpDirNames[i]);
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		path = strdup(raw_path);
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog("    Dir %s\n", path);
		/* XXX: This should be a tree lookup instead of status */
		rc = git_status_file(&flags, ctx->repo, path);
		LGitLog("    Adding %s, git status flags %x\n", path, flags);
		switch (rc) {
		case 0:
			lpStatus[i] = SCC_DIRSTATUS_CONTROLLED;
			break;
		case GIT_ENOTFOUND:
			lpStatus[i] = SCC_DIRSTATUS_NOTCONTROLLED;
			LGitLog("      Not found\n");
			break;
		default:
			lpStatus[i] = SCC_DIRSTATUS_INVALID;
			LGitLibraryError(NULL, "Populate list");
			LGitLog("      Error (%x)\n", rc);
			break;
		}
	}
	return SCC_OK;
}

static DWORD LGitConvertQueryChangesFlags(unsigned int flags)
{
	/*
	 * Protip: changes relative from HEAD to stage/index are INDEX.
	 * Changes relative from stage/index to the working directory are WT.
	 */
	DWORD sccFlags = SCC_CHANGE_UNCHANGED;
	/* Files new to the working tree */
	if (flags & GIT_STATUS_WT_NEW) {
		sccFlags = SCC_CHANGE_LOCAL_ADDED;
	}
	/* Modifications. */
	if ((flags & GIT_STATUS_WT_MODIFIED)
		|| (flags & GIT_STATUS_WT_TYPECHANGE)
		|| (flags & GIT_STATUS_INDEX_MODIFIED)
		|| (flags & GIT_STATUS_INDEX_TYPECHANGE)) {
		sccFlags = SCC_CHANGE_DIFFERENT;
	}
	/* Files deleted by plain delete (rm) or index/stage delete (git rm) */
	if (flags & GIT_STATUS_WT_DELETED) {
		sccFlags = SCC_CHANGE_LOCAL_DELETED; /* or DATABASE_ADDED */
	}
	if (flags & GIT_STATUS_INDEX_DELETED) {
		sccFlags = SCC_CHANGE_DATABASE_DELETED;
	}
	/* XXX: Rename? Is this mapping correct */
	return sccFlags;
}

static int LGitQueryChangesFile(LPCSTR fileName,
								unsigned int lg2flags,
								int lg2rc,
								QUERYCHANGESFUNC cb,
								LPVOID cbData)
{
	QUERYCHANGESDATA qcd;
	ZeroMemory(&qcd, sizeof(QUERYCHANGESDATA));
	qcd.dwSize = sizeof(QUERYCHANGESDATA);
	qcd.lpFileName = fileName;
	switch (lg2rc) {
	case 0:
		qcd.dwChangeType = LGitConvertQueryChangesFlags(lg2flags);
		break;
	case GIT_ENOTFOUND:
		qcd.dwChangeType = SCC_CHANGE_NONEXISTENT;
		LGitLog("      Not found\n");
		break;
	default:
		qcd.dwChangeType = SCC_CHANGE_UNKNOWN;
		LGitLibraryError(NULL, "Populate list");
		LGitLog("      Error (%x)\n", lg2rc);
		break;
	}
	return cb(cbData, &qcd);
}

SCCRTN SccQueryChanges(LPVOID context,
					   LONG nFiles,
					   LPCSTR *lpFileNames,
					   QUERYCHANGESFUNC pfnCallback,
					   LPVOID pvCallerData)
{
	LGitContext *ctx = (LGitContext*)context;
	int i, rc;
	unsigned int flags;
	const char *raw_path;
	LGitLog("**SccQueryChanges** count %d\n", nFiles);
	for (i = 0; i < nFiles; i++) {
		char *path;
		raw_path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", lpFileNames[i]);
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		path = strdup(raw_path);
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog("    %s\n", path);
		rc = git_status_file(&flags, ctx->repo, path);
		LGitLog("    Adding %s, git status flags %x\n", path, flags);
		int ret = LGitQueryChangesFile(path, flags, rc, pfnCallback, pvCallerData);
		if (ret == SCC_I_OPERATIONCANCELED || ret < 0) {
			LGitLog(" ! Callback returned %d\n", ret);
			break;
		}
	}
	return SCC_OK;
}

SCCRTN SccGetEvents (LPVOID context, 
					 LPSTR lpFileName,
					 LPLONG lpStatus,
					 LPLONG pnEventsRemaining)
{
	LGitLog("**SccGetEvents** %s\n", lpFileName);
	return SCC_E_OPNOTSUPPORTED;
}