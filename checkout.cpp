/*
 * Checkouts of various kind. The MSSCCI concept of checkout doesn't actually
 * apply here, but "undo checkout" seems to conceptually map to "checkout to
 * HEAD". Confusing!
 *
 * If an IDE supports multiple checkouts, we can perhaps map SccCheckout to
 * a "controlled" "git checkout" dialog, so they can pick the thing to pull
 * the file from, same with SccGet?
 */

#include "stdafx.h"
#include "LGit.h"

static void LGitCheckoutProgress(const char *path,
								   size_t completed,
								   size_t total,
								   void *buf)
{
	LGitLog(" ! Checking out %s: %d/%d\n", path, completed, total);
}

SCCRTN SccUncheckout (LPVOID context, 
					  HWND hWnd, 
					  LONG nFiles, 
					  LPCSTR* lpFileNames, 
					  LONG dwFlags,
					  LPCMDOPTS pvOptions)
{
	int i, path_count;
	const char **paths, *path;
	git_checkout_options co_opts;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccUncheckout**\n");
	LGitLog("  flags %x", dwFlags);
	LGitLog("  files %d", nFiles);

	git_checkout_options_init(&co_opts, GIT_CHECKOUT_OPTIONS_VERSION);
	co_opts.progress_cb = LGitCheckoutProgress;
	co_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	/* XXX: Apply GIT_CHECKOUT_DONT_WRITE_INDEX? */

	paths = (const char**)calloc(sizeof(char*), nFiles);
	if (paths == NULL) {
		return SCC_E_NONSPECIFICERROR;
	}
	path_count = 0;
	for (i = 0; i < nFiles; i++) {
		path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", lpFileNames[i]);
			continue;
		}
		LGitLog("    %s\n", path);
		paths[path_count++] = path;
	}
	/*
	for (i = 0; i < path_count; i++) {
		LGitLog("    In list, %s\n", path);
	}
	*/
	co_opts.paths.strings = (char**)paths;
	co_opts.paths.count = path_count;

	if (git_checkout_head(ctx->repo, &co_opts) != 0) {
		LGitLibraryError(hWnd, "SccUncheckout git_checkout_head");
		free(paths);
		return SCC_E_NONSPECIFICERROR;
	}

	free(paths);
	return SCC_OK;
}

/*
 * Nops.
 */

SCCRTN SccGet (LPVOID context, 
			   HWND hWnd, 
			   LONG nFiles, 
			   LPCSTR* lpFileNames, 
			   LONG dwFlags,
			   LPCMDOPTS pvOptions)
{
	OutputDebugString("**SccGet**\n");
	// In our case, we should have get be a nop.
	return SCC_OK;
}

SCCRTN SccCheckout (LPVOID context, 
					HWND hWnd, 
					LONG nFiles, 
					LPCSTR* lpFileNames, 
					LPCSTR lpComment, 
					LONG dwFlags,
					LPCMDOPTS pvOptions)
{
	OutputDebugString("**SccCheckout**\n");
	// Nop, we pretend all files are checked out
	// Because they have to with git
	return SCC_OK;
}