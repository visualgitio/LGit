/*
 * Checkouts of various kind. The MSSCCI concept of checkout doesn't actually
 * apply here, but "undo checkout" seems to conceptually map to "checkout to
 * HEAD". Confusing!
 *
 * If an IDE supports multiple checkouts, we can perhaps map SccCheckout to
 * a "controlled" "git checkout" dialog, so they can pick the thing to pull
 * the file from.
 */

#include "stdafx.h"
#include "LGit.h"

static SCCRTN LGitCheckoutInternal (LPVOID context, 
									HWND hWnd, 
									LONG nFiles, 
									LPCSTR* lpFileNames, 
									LONG dwFlags,
									LPCMDOPTS pvOptions)
{
	int i, path_count;
	const char *raw_path;
	char **paths;
	git_checkout_options co_opts;
	LGitContext *ctx = (LGitContext*)context;

	/*
	 * Flags mean files under a directory or recursive directory. This handles
	 * the latter; the former could be done with adjustments in pathspec?
	 * (That is, SCC_GET_ALL or SCC_GET_RECURSIVE)
	 */
	LGitLog("  flags %x", dwFlags);
	LGitLog("  files %d", nFiles);

	git_checkout_options_init(&co_opts, GIT_CHECKOUT_OPTIONS_VERSION);
	co_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	/* XXX: Apply GIT_CHECKOUT_DONT_WRITE_INDEX? */

	paths = (char**)calloc(sizeof(char*), nFiles);
	if (paths == NULL) {
		return SCC_E_NONSPECIFICERROR;
	}
	path_count = 0;
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
		paths[path_count++] = path;
	}
	/*
	for (i = 0; i < path_count; i++) {
		LGitLog("    In list, %s\n", path);
	}
	*/
	co_opts.paths.strings = paths;
	co_opts.paths.count = path_count;

	if (git_checkout_head(ctx->repo, &co_opts) != 0) {
		LGitLibraryError(hWnd, "SccUncheckout git_checkout_head");
		LGitFreePathList(paths, path_count);
		return SCC_E_NONSPECIFICERROR;
	}

	LGitFreePathList(paths, path_count);
	return SCC_OK;
}

/**
 * Basically undoes modified changes by doing equivalent of "git checkout".
 */
SCCRTN SccUncheckout (LPVOID context, 
					  HWND hWnd, 
					  LONG nFiles, 
					  LPCSTR* lpFileNames, 
					  LONG dwFlags,
					  LPCMDOPTS pvOptions)
{
	LGitLog("**SccUncheckout**\n");
	return LGitCheckoutInternal(context, hWnd, nFiles, lpFileNames, dwFlags, pvOptions);
}

/**
 * Also a "git checkout", but can be done for unmodified/deleted/etc. files,
 * because SccUncheckout will only be shown for files that SccQueryInfo
 * returned checked out on (modified).
 *
 * As an example, if a file has been just "rm"ed, then SccGit may be the only
 * option exposed in the IDE.
 */
SCCRTN SccGet (LPVOID context, 
			   HWND hWnd, 
			   LONG nFiles, 
			   LPCSTR* lpFileNames, 
			   LONG dwFlags,
			   LPCMDOPTS pvOptions)
{
	LGitLog("**SccGet**\n");
	return LGitCheckoutInternal(context, hWnd, nFiles, lpFileNames, dwFlags, pvOptions);
}

SCCRTN SccCheckout (LPVOID context, 
					HWND hWnd, 
					LONG nFiles, 
					LPCSTR* lpFileNames, 
					LPCSTR lpComment, 
					LONG dwFlags,
					LPCMDOPTS pvOptions)
{
	int i;
	LGitLog("**SccCheckout**\n");
	LGitLog("  flags %x", dwFlags);
	LGitLog("  files %d", nFiles);
	/*
	 * VB6 and especially VS.NET will mark files read-only after checkin or
	 * uncheckout. Try to unmark the files and see if the IDE is OK with it.
	 * XXX: Maybe not the best fix
	 */
	for (i = 0; i < nFiles; i++) {
		DWORD attr;
		attr = GetFileAttributes(lpFileNames[i]);
		if (attr == INVALID_FILE_ATTRIBUTES) {
			continue;
		}
		attr &= ~FILE_ATTRIBUTE_READONLY;
		SetFileAttributes(lpFileNames[i], attr);
	}
	return SCC_OK;
}