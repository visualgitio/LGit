/*
 * Project management functions that handle the connection. Done before and
 * after initialization and capabilities.
 */

#include "stdafx.h"

static SCCRTN LGitInitRepo(HWND hWnd, LPSTR lpProjName, LPCSTR lpLocalPath)
{
	/* The repository is created, but we'll re-open in SccOpenProject */
	git_repository *temp_repo;
	int res;
	char msg[512];
	LGitLog(" ! Init/Import\n");
	_snprintf(msg, 512,
		"There is no Git repository in '%s' for the project '%s'. "
		"Would you like to create one?",
		lpLocalPath, lpProjName);
	res = MessageBox(hWnd, msg, "Initialize Git Repository",
		MB_ICONQUESTION | MB_YESNO);
	switch (res) {
	case IDYES:
		/* XXX: use the _ext variant */
		if (git_repository_init(&temp_repo, lpLocalPath, 0) != 0) {
			LGitLibraryError(hWnd, "Repo Init");
			return SCC_E_UNKNOWNERROR;
		}
		git_repository_free(temp_repo);
		LGitGetProjectNameFromPath(lpProjName, lpLocalPath, SCC_PRJPATH_SIZE);
		LGitLog(" ! New proj name is %s\n", lpProjName);
		return SCC_OK;
	case IDNO:
		return SCC_I_OPERATIONCANCELED;
	default:
		return SCC_E_UNKNOWNERROR;
	}
}

LGIT_API SCCRTN LGitOpenProject(LPVOID context,
								HWND hWnd, 
								LPSTR lpUser,
								LPSTR lpProjName, /* writeable, contrary to MSDN */
								LPCSTR lpLocalProjPath,
								LPSTR lpAuxProjPath,
								LPCSTR lpComment,
								LPTEXTOUTPROC lpTextOutProc,
								LONG dwFlags)
{
	int rc;
	const char *workdir;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**LGitOpenProject** Context=%p\n", ctx);
	LGitLog("  user %s\n", lpUser);
	LGitLog("  proj name %s\n", lpProjName);
	LGitLog("  local proj path %s\n", lpLocalProjPath);
	LGitLog("  aux proj path %s\n", lpAuxProjPath);
	LGitLog("  comment %s\n", lpComment);
	LGitLog("  flags %x\n", dwFlags);

	if (ctx->active) {
		LGitLog(" ! Context already is active\n");
	}

	// If flags & 1, it's probably init, but it could be called from existing
	BOOL havent_init = TRUE;
init_again:
	rc = git_repository_open_ext(&ctx->repo, lpLocalProjPath, 0, NULL);
	if (rc == 0) {
		LGitLog("    Got it\n");
		// Repo already exists, connect to it
		strlcpy(ctx->path, lpLocalProjPath, 1024);
	} else if (rc == GIT_ENOTFOUND && dwFlags & SCC_OP_CREATEIFNEW && havent_init) {
		// No repository, create/clone depending on bAllowChangePath
		LGitLog("    Initializing\n");
		/* This is ignored */
		char proj[SCC_PRJPATH_SIZE];
		SCCRTN init_ret = LGitInitRepo(hWnd, proj, lpLocalProjPath);
		if (init_ret != 0) {
			return SCC_E_COULDNOTCREATEPROJECT;
		}
		goto init_again;
	} else if (rc == GIT_ENOTFOUND) {
		LGitLog("    No repo\n");
		return SCC_E_UNKNOWNPROJECT;
	} else if (rc == -1) {
		// Error opening
		LGitLibraryError(hWnd, "SccOpenProject");
		return SCC_E_UNKNOWNERROR;
	}

	workdir = git_repository_workdir(ctx->repo);
	if (workdir == NULL) {
		LGitLog("!! Workdir doesn't exist, bare repo");
		MessageBox(hWnd, "The repository is bare.", "Can't open repository",
			MB_ICONERROR);
		return SCC_E_INVALIDFILEPATH;
	}
	strlcpy(ctx->workdir_path, workdir, 1024);
	/*
	 * Translate the path to Windows-style backslashes. The IDE will return us
	 * backslashes, which will confuse PathCommonPrefix. We translate them back
	 * after the prefix check in each function.
	 */
	LGitTranslateStringChars(ctx->workdir_path, '/', '\\');
	LGitUtf8ToWide(ctx->workdir_path, ctx->workdir_path_utf16, 1024);
	LGitLog("  The workdir is %s\n", ctx->workdir_path);

	ctx->active = TRUE;
	ctx->textoutCb = lpTextOutProc;
	strlcpy(ctx->username, lpUser, SCC_USER_LEN);
	LGitGetProjectNameFromPath(lpProjName, ctx->workdir_path, SCC_PRJPATH_SIZE);
	LGitLog(" ! New proj name is %s\n", lpProjName);

	/* XXX: should init/deinit at project level? */
	ctx->checkouts = new CheckoutQueue();
	
	LGitInitializeFonts(ctx);

	return SCC_OK;
}

/*
 * Wrapper that converts to ANSI, because it would be unmanageable in the
 * normal function due to how project name stuff is handled.
 */
SCCRTN SccOpenProject (LPVOID context,
					   HWND hWnd, 
					   LPSTR lpUser,
					   LPSTR lpProjName, /* writeable, contrary to MSDN */
					   LPCSTR lpLocalProjPath,
					   LPSTR lpAuxProjPath,
					   LPCSTR lpComment,
					   LPTEXTOUTPROC lpTextOutProc,
					   LONG dwFlags)
{
	char user[SCC_USER_SIZE];
	char projName[SCC_NAME_SIZE];
	char auxProjPath[SCC_PRJPATH_SIZE]; /* not auxlabel, according to hdr */
	char localProjPath[SCC_PRJPATH_SIZE];
	/* XXX: comment? not used yet */
	LGitAnsiToUtf8(lpUser, user, SCC_USER_SIZE);
	LGitAnsiToUtf8(lpProjName, projName, SCC_NAME_SIZE);
	LGitAnsiToUtf8(lpAuxProjPath, auxProjPath, SCC_PRJPATH_SIZE);
	/* r/o and not set after */
	LGitAnsiToUtf8(lpLocalProjPath, localProjPath, SCC_PRJPATH_SIZE);
	SCCRTN ret = LGitOpenProject(context, hWnd, user, projName, localProjPath, auxProjPath, lpComment, lpTextOutProc, dwFlags);
	LGitUtf8ToAnsi(user, lpUser, SCC_USER_SIZE);
	LGitUtf8ToAnsi(projName, lpProjName, SCC_NAME_SIZE);
	LGitUtf8ToAnsi(auxProjPath, lpAuxProjPath, SCC_PRJPATH_SIZE);
	return ret;
}

SCCRTN SccCloseProject (LPVOID context)
{
	LGitContext *ctx = (LGitContext*)context;
	LGitLog("**SccCloseProject** Context=%p\n", ctx);
	LGitLog("    Active? %d\n", ctx->active);
	if (context) {
		LGitContext *ctx = (LGitContext*)context;
		/* additional debug logs because VS traps segfault */
		LGitUninitializeFonts(ctx);
		if (ctx->repo) {
			LGitLog(" ! Free repo\n");
			git_repository_free(ctx->repo);
			ctx->repo = NULL;
		}
		if (ctx->checkouts) {
			LGitLog(" ! Free checkouts\n");
			delete ctx->checkouts;
			ctx->checkouts = NULL;
		}
		ctx->renameCb = NULL;
		ctx->renameData = NULL;
		ctx->textoutCb = NULL;
		ZeroMemory(ctx->path, 1024);
		ZeroMemory(ctx->workdir_path, 1024);
		ZeroMemory(ctx->workdir_path_utf16, 1024 * sizeof(wchar_t));
		ctx->active = FALSE;
		LGitLog(" ! Cleared, now inactive\n");
	}
	return SCC_OK;
}

LGIT_API SCCRTN LGitGetProjPath(LPVOID context, 
								HWND hWnd, 
								LPSTR lpUser,
								LPSTR lpProjName, 
								LPSTR lpLocalPath,
								LPSTR lpAuxProjPath,
								BOOL bAllowChangePath,
								LPBOOL pbNew)
{
	int rc;
	git_repository *temp_repo;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**LGitGetProjPath** Context=%p\n", context);
	LGitLog("  user %s\n", lpUser);
	LGitLog("  proj name %s\n", lpProjName);
	LGitLog("  local path %s\n", lpLocalPath);
	LGitLog("  aux proj path %s\n", lpAuxProjPath);
	LGitLog("  allow change path? %x\n", bAllowChangePath);
	LGitLog("  can make new? %x\n", *pbNew);

	/* XXX: Employ git_repository_discover */
	/* XXX: Also deref pbNew since it can be set incoming */
	if (bAllowChangePath) {
		// Can change path, probably clone
		return LGitClone(ctx, hWnd, lpProjName, lpLocalPath, pbNew);
	}

	// If bAllowChangePath is called, we're likely cloning.
	// Otherwise, we're initing probably. (or perhaps binding?)
	// The init case is probably when SccOpenProject is called with 1.
	rc = git_repository_open_ext(&temp_repo, lpLocalPath, 0, NULL);
	if (rc == 0) {
		// Repo already exists, connect to it
		LGitLog(" ! Repo exists, connecting\n");
		git_repository_free(temp_repo);
		LGitGetProjectNameFromPath(lpProjName, lpLocalPath, SCC_PRJPATH_SIZE);
		LGitLog(" ! New proj name is %s\n", lpProjName);
	} else if (rc == GIT_ENOTFOUND && !bAllowChangePath) {
		// Can't change path, probably initing and importing existing files
		return LGitInitRepo(hWnd, lpProjName, lpLocalPath);
	} else if (rc == -1) {
		// Error opening
		LGitLibraryError(hWnd, "SccGetProjPath");
		return SCC_E_UNKNOWNERROR;
	}

	return SCC_OK;
}

SCCRTN SccGetProjPath (LPVOID context, 
					   HWND hWnd, 
					   LPSTR lpUser,
					   LPSTR lpProjName, 
					   LPSTR lpLocalPath,
					   LPSTR lpAuxProjPath,
					   BOOL bAllowChangePath,
					   LPBOOL pbNew)
{
	char user[SCC_USER_SIZE]; /* not used but in case */
	char projName[SCC_NAME_SIZE];
	char auxProjPath[SCC_PRJPATH_SIZE]; /* not auxlabel, according to hdr */
	char localProjPath[SCC_PRJPATH_SIZE];
	LGitAnsiToUtf8(lpUser, user, SCC_USER_SIZE);
	LGitAnsiToUtf8(lpProjName, projName, SCC_NAME_SIZE);
	LGitAnsiToUtf8(lpAuxProjPath, auxProjPath, SCC_PRJPATH_SIZE);
	LGitAnsiToUtf8(lpLocalPath, localProjPath, SCC_PRJPATH_SIZE);
	SCCRTN ret = LGitGetProjPath(context, hWnd, user, projName, localProjPath, auxProjPath, bAllowChangePath, pbNew);
	LGitUtf8ToAnsi(user, lpUser, SCC_USER_SIZE);
	LGitUtf8ToAnsi(projName, lpProjName, SCC_NAME_SIZE);
	LGitUtf8ToAnsi(auxProjPath, lpAuxProjPath, SCC_PRJPATH_SIZE);
	/* can be modified */
	if (bAllowChangePath) {
		LGitUtf8ToAnsi(localProjPath, lpLocalPath, SCC_PRJPATH_SIZE);
	}
	return ret;
}

/* Here be dragons (subprojects) */
SCCRTN SccGetParentProjectPath(LPVOID context,
							   HWND hWnd,
							   LPSTR lpUser,
							   LPCSTR lpProjPath,
							   LPSTR  lpAuxProjPath,
							   LPSTR  lpParentProjPath)
{
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccGetParentProjectPath** Context=%p\n", context);
	LGitLog("  user %s\n", lpUser);
	LGitLog("  proj path %s\n", lpProjPath);
	LGitLog("  aux path (inout) %s\n", lpAuxProjPath);
	LGitLog("  parent proj path (inout) %s\n", lpParentProjPath);
	return SCC_E_OPNOTSUPPORTED;
}

SCCRTN SccCreateSubProject(LPVOID context,
						   HWND hWnd,
						   LPSTR lpUser,
						   LPCSTR lpParentProjPath,
						   LPCSTR lpSubProjName,
						   LPSTR lpAuxProjPath,
						   LPSTR lpSubProjPath)
{
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccCreateSubProject** Context=%p\n", context);
	LGitLog("  user %s\n", lpUser);
	LGitLog("  parent proj path %s\n", lpParentProjPath);
	LGitLog("  subproject name %s\n", lpSubProjName);
	LGitLog("  aux path (inout) %s\n", lpAuxProjPath);
	LGitLog("  subproject path (inout) %s\n", lpSubProjPath);
	return SCC_E_OPNOTSUPPORTED;
}
