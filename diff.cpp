/*
 * Diffs between files and directories.
 */

#include "stdafx.h"
#include "LGit.h"

SCCRTN LGitDiffInternal (LPVOID context, 
						 HWND hWnd, 
						 LPCSTR lpFileName, 
						 LONG dwFlags,
						 LPCMDOPTS pvOptions)
{
	git_diff_options diffopts;
	git_diff *diff;
	size_t deltas;
	
	LGitDiffDialogParams params;

	const char *raw_path;
	char path[1024], *path_ptr;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("  Flags %x, %s\n", dwFlags, lpFileName);

	git_diff_options_init(&diffopts, GIT_DIFF_OPTIONS_VERSION);

	raw_path = LGitStripBasePath(ctx, lpFileName);
	if (raw_path == NULL) {
		LGitLog("     Couldn't get base path for %s\n", lpFileName);
		return SCC_E_NONSPECIFICERROR;
	}
	/* Translate because libgit2 operates with forward slashes */
	strlcpy(path, raw_path, 1024);
	LGitTranslateStringChars(path, '\\', '/');

	/* Work around a very weird C quirk where &array == array are the same */
	path_ptr = path;
	diffopts.pathspec.strings = &path_ptr;
	diffopts.pathspec.count = 1;

	if (dwFlags & SCC_DIFF_IGNORESPACE) {
		diffopts.flags |= GIT_DIFF_IGNORE_WHITESPACE;
	}
	/* XXX: Not sure if semantics are a precise match */
	if (dwFlags & SCC_DIFF_QD_CHECKSUM) {
		diffopts.flags |= GIT_DIFF_FIND_EXACT_MATCH_ONLY;
	}

	/*
	 * Conceptually, SccDiff is "what's different from the commited copy".
	 * This is similar to a straight "git diff" with a specific file, that is,
	 * we're comparing the working tree to HEAD.
	 */
	if (git_diff_index_to_workdir(&diff, ctx->repo, NULL, &diffopts) != 0) {
		LGitLibraryError(hWnd, "SccDiff git_diff_index_to_workdir");
		return SCC_E_NONSPECIFICERROR;
	}
	/* XXX: Rename detection with git_diff_find_similar? */
	deltas = git_diff_num_deltas(diff);

	/* If it's a quick diff, don't pop up a UI */
	if (dwFlags & SCC_DIFF_QUICK_DIFF)
	{
		/* Contents only. We don't (yet?) support timestamp diff */
		goto fin;
	}

	params.ctx = ctx;
	params.diff = diff;
	params.path = path;
	switch (LGitDiffWindow(hWnd, &params)) {
	case 0:
		LGitLog(" ! Uh-oh, dialog error\n");
		git_diff_free(diff);
		return SCC_E_NONSPECIFICERROR;
	default:
		break;
	}

fin:
	git_diff_free(diff);
	return deltas > 0 ? SCC_I_FILEDIFFERS : SCC_OK;
}

/*
 * The LGitDiffInternal has the same semantics regardless if it's a file or a
 * directory being compared. Traditional SCC plugins would probably do a side
 * by side comparison of the file or directory, but Git doesn't really work
 * like that. Instead, we'll just shove it all into the same pathspec.
 */
SCCRTN SccDiff (LPVOID context, 
				HWND hWnd, 
				LPCSTR lpFileName, 
				LONG dwFlags,
				LPCMDOPTS pvOptions)
{
	LGitLog("**SccDiff**\n");
	return LGitDiffInternal(context, hWnd, lpFileName, dwFlags, pvOptions);
}

SCCRTN SccDirDiff (LPVOID context, 
				   HWND hWnd, 
				   LPCSTR lpFileName, 
				   LONG dwFlags,
				   LPCMDOPTS pvOptions)
{
	LGitLog("**SccDirDiff**\n");
	return LGitDiffInternal(context, hWnd, lpFileName, dwFlags, pvOptions);
}