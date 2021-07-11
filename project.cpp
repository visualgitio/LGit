/*
 * Project management functions that handle the connection. Done before and
 * after initialization and capabilities.
 */

#include "stdafx.h"
#include "LGit.h"

SCCRTN SccOpenProject (LPVOID context,
					   HWND hWnd, 
					   LPSTR lpUser,
					   LPSTR lpProjName,
					   LPCSTR lpLocalProjPath,
					   LPSTR lpAuxProjPath,
					   LPCSTR lpComment,
					   LPTEXTOUTPROC lpTextOutProc,
					   LONG dwFlags)
{
	int rc;
	const char *workdir;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccOpenProject**\n");
	LGitLog("  user %s\n", lpUser);
	LGitLog("  proj name %s\n", lpProjName);
	LGitLog("  local proj path %s\n", lpLocalProjPath);
	LGitLog("  aux proj path %s\n", lpAuxProjPath);
	LGitLog("  comment %s\n", lpComment);
	LGitLog("  flags %x\n", dwFlags);

	// If flags & 1, it's probably init, but it could be called from existing
	rc = git_repository_open_ext(&ctx->repo, lpLocalProjPath, 0, NULL);
	if (rc == 0) {
		LGitLog("    Got it\n");
		// Repo already exists, connect to it
		strcpy(ctx->path, lpLocalProjPath);
	} else if (rc == GIT_ENOTFOUND) {
		// No repository, create/clone depending on bAllowChangePath
		LGitLog("    No git repo\n");
		return SCC_E_UNKNOWNERROR;
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
	strncpy(ctx->workdir_path, workdir, 1024);
	/*
	 * Translate the path to Windows-style backslashes. The IDE will return us
	 * backslashes, which will confuse PathCommonPrefix. We translate them back
	 * after the prefix check in each function.
	 */
	LGitTranslateStringChars(ctx->workdir_path, '/', '\\');
	LGitLog("  The workdir is %s\n", ctx->workdir_path);

	ctx->active = TRUE;
	return SCC_OK;
}

SCCRTN SccCloseProject (LPVOID context)
{
	LGitContext *ctx = (LGitContext*)context;
	LGitLog("**SccCloseProject** Active? %d\n", ctx->active);
	if (context) {
		LGitContext *ctx = (LGitContext*)context;
		if (ctx->repo) {
			git_repository_free(ctx->repo);
			ctx->repo = NULL;
		}
		ctx->renameCb = NULL;
		ZeroMemory(ctx->path, MAX_PATH + 1);
		ctx->active = FALSE;
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
	int rc;
	git_repository *temp_repo;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccGetProjPath**\n");
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
		LGitLog(" ! Clone\n");
		MessageBox(hWnd, "Not implemented yet", "Clone Git Repository",
			MB_ICONWARNING);
		return SCC_E_UNKNOWNERROR;
	}

	// If bAllowChangePath is called, we're likely cloning.
	// Otherwise, we're initing probably. (or perhaps binding?)
	// The init case is probably when SccOpenProject is called with 1.
	rc = git_repository_open_ext(&temp_repo, lpLocalPath, 0, NULL);
	if (rc == 0) {
		// Repo already exists, connect to it
		LGitLog(" ! Repo exists, connecting\n");
		git_repository_free(temp_repo);
	} else if (rc == GIT_ENOTFOUND && !bAllowChangePath) {
		// Can't change path, probably initing and importing existing files
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
			/* XXX: Break into sep func, use the _ext variant */
			if (git_repository_init(&temp_repo, lpLocalPath, 0) != 0) {
				LGitLibraryError(hWnd, "SccGetProjPath Repo Init");
				return SCC_E_UNKNOWNERROR;
			}
			git_repository_free(temp_repo);
			return SCC_OK;
		case IDNO:
			return SCC_I_OPERATIONCANCELED;
		}
	} else if (rc == -1) {
		// Error opening
		LGitLibraryError(hWnd, "SccGetProjPath");
		return SCC_E_UNKNOWNERROR;
	}

	return SCC_OK;
}