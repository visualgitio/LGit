/*
 * Diffs between files and directories.
 */

#include "stdafx.h"

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
	LGitInitDiffProgressCallback(ctx, &diffopts);

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
	LGitProgressInit(ctx, "Diffing Stage to Working Tree", 0);
	LGitProgressStart(ctx, hWnd, FALSE);
	if (git_diff_index_to_workdir(&diff, ctx->repo, NULL, &diffopts) != 0) {
		LGitProgressDeinit(ctx);
		LGitLibraryError(hWnd, "SccDiff git_diff_index_to_workdir");
		return SCC_E_NONSPECIFICERROR;
	}
	LGitProgressDeinit(ctx);
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
	params.commit = NULL;
	switch (LGitDiffWindow(hWnd, &params)) {
	case 0:
	case -1:
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
	LGitLog("**SccDiff** Context=%p\n", context);
	return LGitDiffInternal(context, hWnd, lpFileName, dwFlags, pvOptions);
}

SCCRTN SccDirDiff (LPVOID context, 
				   HWND hWnd, 
				   LPCSTR lpFileName, 
				   LONG dwFlags,
				   LPCMDOPTS pvOptions)
{
	LGitLog("**SccDirDiff** Context=%p\n", context);
	return LGitDiffInternal(context, hWnd, lpFileName, dwFlags, pvOptions);
}

SCCRTN LGitCommitToCommitDiff(LGitContext *ctx,
							  HWND hwnd,
							  git_commit *commit_b,
							  git_commit *commit_a,
							  git_diff_options *diffopts)
{
	LGitLog("**LGitCommitToCommitDiff** Context=%p\n", ctx);
	SCCRTN ret = SCC_OK;
	git_tree *a, *b;
	const git_oid *oid_a, *oid_b;
	oid_a = git_commit_id(commit_a);
	LGitLog("  A %s\n", git_oid_tostr_s(oid_a));
	oid_b = git_commit_id(commit_b);
	LGitLog("  B %s\n", git_oid_tostr_s(oid_b));
	git_diff *diff;
	if (git_commit_tree(&a, commit_a) != 0) {
		LGitLibraryError(hwnd, "match_with_parent git_commit_tree A");
		goto fin;
	}
	if (git_commit_tree(&b, commit_b) != 0) {
		LGitLibraryError(hwnd, "match_with_parent git_commit_tree B");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	/* XXX: ugly because we don't know if we have callbacks or not */
	if (diffopts->progress_cb != NULL) {
		LGitProgressInit(ctx, "Diffing Commits", 0);
		LGitProgressStart(ctx, hwnd, FALSE);
		/* it is safe to call uninit without guard, but leaves a message */
	}
	if (git_diff_tree_to_tree(&diff, git_commit_owner(commit_b), a, b, diffopts) != 0) {
		if (diffopts->progress_cb != NULL) {
			LGitProgressDeinit(ctx);
		}
		LGitLibraryError(hwnd, "match_with_parent git_diff_tree_to_tree");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	if (diffopts->progress_cb != NULL) {
		LGitProgressDeinit(ctx);
	}
	/* make a title */
	char msg[128];
	_snprintf(msg, 128, "%s..", git_oid_tostr_s(oid_a));
	strlcat(msg, git_oid_tostr_s(oid_b), 128);
	/* now display the dialog */
	LGitDiffDialogParams diff_params;
	diff_params.ctx = ctx;
	diff_params.diff = diff;
	diff_params.path = msg;
	diff_params.commit = commit_b;
	switch (LGitDiffWindow(hwnd, &diff_params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	default:
		break;
	}
fin:
	if (diff != NULL) {
		git_diff_free(diff);
	}
	if (a != NULL) {
		git_tree_free(a);
	}
	if (b != NULL) {
		git_tree_free(b);
	}
	return ret;
}

SCCRTN LGitCommitToParentDiff(LGitContext *ctx,
							  HWND hwnd,
							  git_commit *commit,
							  git_diff_options *diffopts)
{
	LGitLog("**LGitCommitToCommitDiff** Context=%p\n", ctx);
	SCCRTN ret = SCC_OK;
	git_commit *parent;
	int parents = (int)git_commit_parentcount(commit);
	if (parents < 1) {
		MessageBox(hwnd,
			"There are no parents to compare the commit against.",
			"Can't Display Diff",
			MB_ICONERROR);
		goto fin;
	}
	/* XXX: We assume the first parent */
	if (git_commit_parent(&parent, commit, 0) != 0) {
		LGitLibraryError(hwnd, "match_with_parent git_commit_parent");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	ret = LGitCommitToCommitDiff(ctx, hwnd, commit, parent, diffopts);
fin:
	if (parent != NULL) {
		git_commit_free(parent);
	}
	return ret;
}

SCCRTN LGitDiffStageToWorkdir(LGitContext *ctx, HWND hwnd, git_strarray *paths)
{
	LGitLog("**LGitCommitToCommitDiff** Context=%p\n", ctx);
	SCCRTN ret = SCC_OK;
	LGitDiffDialogParams params;
	ZeroMemory(&params, sizeof(LGitDiffDialogParams));
	git_diff *diff = NULL;
	/* XXX: config opts since we have no flags? allow passing in? */
	git_diff_options diffopts;
	git_diff_options_init(&diffopts, GIT_DIFF_OPTIONS_VERSION);
	if (paths != NULL) {
		diffopts.pathspec.strings = paths->strings;
		diffopts.pathspec.count = paths->count;
	}
	LGitInitDiffProgressCallback(ctx, &diffopts);
	/* repo index on null */
	LGitProgressInit(ctx, "Diffing Stage to Working Tree", 0);
	LGitProgressStart(ctx, hwnd, FALSE);
	if (git_diff_index_to_workdir(&diff, ctx->repo, NULL, &diffopts) != 0) {
		LGitLibraryError(hwnd, "git_diff_index_to_workdir");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	LGitProgressDeinit(ctx);

	params.ctx = ctx;
	params.diff = diff;
	params.path = "Stage to Working Directory";
	params.commit = NULL;
	switch (LGitDiffWindow(hwnd, &params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	default:
		break;
	}
fin:
	if (diff != NULL) {
		git_diff_free(diff);
	}
	return ret;
}

/* XXX: Assume peelable object? */
SCCRTN LGitDiffTreeToWorkdir(LGitContext *ctx, HWND hwnd, git_strarray *paths, git_tree *tree)
{
	LGitLog("**LGitDiffTreeToWorkdir** Context=%p\n", ctx);
	SCCRTN ret = SCC_OK;
	LGitDiffDialogParams params;
	ZeroMemory(&params, sizeof(LGitDiffDialogParams));
	git_diff *diff = NULL;
	/* XXX: config opts since we have no flags? allow passing in? */
	git_diff_options diffopts;
	git_diff_options_init(&diffopts, GIT_DIFF_OPTIONS_VERSION);
	if (paths != NULL) {
		diffopts.pathspec.strings = paths->strings;
		diffopts.pathspec.count = paths->count;
	}
	LGitInitDiffProgressCallback(ctx, &diffopts);
	LGitProgressInit(ctx, "Diffing Tree to Working Tree", 0);
	LGitProgressStart(ctx, hwnd, FALSE);
	if (git_diff_tree_to_workdir(&diff, ctx->repo, tree, &diffopts) != 0) {
		LGitLibraryError(hwnd, "git_diff_index_to_workdir");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	LGitProgressDeinit(ctx);

	char title[128];
	if (tree != NULL) {
		const git_oid *tree_oid = git_tree_id(tree);
		_snprintf(title, 128, "%s to Working Directory", git_oid_tostr_s(tree_oid));
		params.path = title;
	} else {
		params.path = "Empty Tree to Working Directory";
	}
	params.ctx = ctx;
	params.diff = diff;
	params.commit = NULL;
	switch (LGitDiffWindow(hwnd, &params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	default:
		break;
	}
fin:
	if (diff != NULL) {
		git_diff_free(diff);
	}
	return ret;
}