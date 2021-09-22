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

static int AskIfCreateBranchFromRemote(HWND hwnd)
{
	return MessageBox(hwnd,
		"The remote branch must have a local branch in order to become HEAD. "
		"Not doing so will detach HEAD. "
		"Would you like to create a local branch?\r\n\r\n"
		"(If you already have a local branch, select it instead of this one.)",
		"Can't Make Remote Branch HEAD",
		MB_ICONQUESTION | MB_YESNOCANCEL);
}

SCCRTN LGitCheckoutRefByName(LGitContext *ctx,
							 HWND hwnd,
							 const char *name)
{
	LGitLog("**LGitCheckoutRefByName** Context=%p\n", ctx);
	LGitLog("  refname %s\n", name);
	/* Resolve the name to a commit for checkout. Example uses annotated... */
	BOOL attached = TRUE;
	int remote_question;
	SCCRTN ret = SCC_E_NONSPECIFICERROR;
	git_reference *branch = NULL, *new_branch = NULL, *checkout_branch;
	git_oid commit_oid;
	git_commit *commit = NULL;
	git_checkout_options co_opts;
	/* For future reference */
	if (git_reference_lookup(&branch, ctx->repo, name) != 0) {
		LGitLibraryError(hwnd, "git_repository_set_head");
		goto err;
	}
	if (git_reference_name_to_id(&commit_oid, ctx->repo, name) != 0) {
		LGitLibraryError(hwnd, "git_reference_name_to_id");
		goto err;
	}
	if (git_commit_lookup(&commit, ctx->repo, &commit_oid) != 0) {
		LGitLibraryError(hwnd, "git_commit_lookup");
		goto err;
	}
	git_checkout_options_init(&co_opts, GIT_CHECKOUT_OPTIONS_VERSION);
	LGitInitCheckoutProgressCallback(ctx, &co_opts);
	/* XXX: Allow setting force */
	co_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	/* If we have a remote branch, create a local one from it. */
	checkout_branch = branch;
	if (git_reference_is_remote(branch)
		/* Ask the user if they want a local branch from it. */
		&& (remote_question = AskIfCreateBranchFromRemote(hwnd)) == IDYES) {
		LGitLog(" ! %s is remote tracking\n", name);
		/* Try to create a local branch from the remote that can be HEAD. */
		int rc = git_branch_create(&new_branch,
			ctx->repo,
			git_reference_shorthand(branch),
			commit,
			0);
		/* XXX: Check if it's because it already exists */
		if (rc != 0) {
			LGitLog("!! Failed to make local tracking branch (%x)\n", rc);
			attached = FALSE;
		} else {
			LGitLog(" ! Made local tracking branch\n");
			checkout_branch = new_branch;
		}
	} else if (!git_reference_is_branch(branch)) {
		/* If it's a tag or similar, we can't track HEAD with it. */
		LGitLog(" ! %s is not a local branch, no attach\n", name);
		attached = FALSE;
	}
	/* React if it's a remote and the user cancelled */
	if (remote_question == IDCANCEL) {
		ret = SCC_I_OPERATIONCANCELED;
		goto err;
	}
	/* Peeled to a tree */
	LGitProgressInit(ctx, "Checking Out Files", 0);
	LGitProgressStart(ctx, hwnd, TRUE);
	if (git_checkout_tree(ctx->repo, (const git_object *)commit, &co_opts) != 0) {
		LGitProgressDeinit(ctx);
		LGitLibraryError(hwnd, "git_checkout_tree");
		goto err;
	}
	/* This must be a local branch to become HEAD. */
	if (attached && git_repository_set_head(ctx->repo, git_reference_name(checkout_branch)) != 0) {
		LGitLog("!! Attached head fail\n");
		/* XXX: Show error */
		MessageBox(hwnd,
			"Failed to set the new attached head. "
			"Attempting to proceed with a detached head.",
			"Couldn't Set HEAD",
			MB_ICONERROR);
		attached = FALSE;
	}
	if (!attached && git_repository_set_head_detached(ctx->repo, &commit_oid) != 0) {
		LGitProgressDeinit(ctx);
		LGitLibraryError(hwnd, "git_repository_set_head_detached");
		goto err;
	}
	LGitProgressDeinit(ctx);
	ret = SCC_OK;
err:
	if (new_branch != NULL) {
		git_reference_free(new_branch);
	}
	if (branch != NULL) {
		git_reference_free(branch);
	}
	if (commit != NULL) {
		git_commit_free(commit);
	}
	return ret;
}

/**
 * This takes an OID that should be peeled to a tree, like a commit.
 */
SCCRTN LGitCheckoutTree(LGitContext *ctx,
						HWND hwnd,
						const git_oid *commit_oid)
{
	LGitLog("**LGitCheckoutTree** Context=%p\n", ctx);
	LGitLog("  oid %s\n", git_oid_tostr_s(commit_oid));
	SCCRTN ret = SCC_E_NONSPECIFICERROR;
	git_commit *commit = NULL;
	git_checkout_options co_opts;
	if (git_commit_lookup(&commit, ctx->repo, commit_oid) != 0) {
		LGitLibraryError(hwnd, "git_commit_lookup");
		goto err;
	}
	git_checkout_options_init(&co_opts, GIT_CHECKOUT_OPTIONS_VERSION);
	LGitInitCheckoutProgressCallback(ctx, &co_opts);
	/* XXX: Allow setting force */
	co_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	/* Peeled to a tree */
	LGitProgressInit(ctx, "Checking Out Files", 0);
	LGitProgressStart(ctx, hwnd, TRUE);
	if (git_checkout_tree(ctx->repo, (const git_object *)commit, &co_opts) != 0) {
		LGitProgressDeinit(ctx);
		LGitLibraryError(hwnd, "git_checkout_tree");
		goto err;
	}
	if (git_repository_set_head_detached(ctx->repo, commit_oid) != 0) {
		LGitProgressDeinit(ctx);
		LGitLibraryError(hwnd, "git_repository_set_head_detached");
		goto err;
	}
	LGitProgressDeinit(ctx);
	ret = SCC_OK;
err:
	if (commit != NULL) {
		git_commit_free(commit);
	}
	return ret;
}

SCCRTN LGitCheckoutStaged(LGitContext *ctx, HWND hwnd, git_strarray *paths)
{
	LGitLog("**LGitCheckoutStaged** Context=%p\n");
	LGitLog("  paths count %u\n", paths->count);
	
	git_checkout_options co_opts;
	git_checkout_options_init(&co_opts, GIT_CHECKOUT_OPTIONS_VERSION);
	LGitInitCheckoutProgressCallback(ctx, &co_opts);
	co_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	co_opts.paths.strings = paths->strings;
	co_opts.paths.count = paths->count;

	LGitProgressInit(ctx, "Checking Out Files", 0);
	LGitProgressStart(ctx, hwnd, TRUE);
	if (git_checkout_index(ctx->repo, NULL, &co_opts) != 0) {
		LGitProgressDeinit(ctx);
		LGitLibraryError(hwnd, "LGitCheckoutStaged git_checkout_index");
		return SCC_E_NONSPECIFICERROR;
	}

	LGitProgressDeinit(ctx);
	return SCC_OK;
}

SCCRTN LGitCheckoutHead(LGitContext *ctx, HWND hwnd, git_strarray *paths)
{
	LGitLog("**LGitStageCheckoutHead** Context=%p\n");
	LGitLog("  paths count %u\n", paths->count);
	
	git_checkout_options co_opts;
	git_checkout_options_init(&co_opts, GIT_CHECKOUT_OPTIONS_VERSION);
	LGitInitCheckoutProgressCallback(ctx, &co_opts);
	co_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	co_opts.paths.strings = paths->strings;
	co_opts.paths.count = paths->count;

	LGitProgressInit(ctx, "Checking Out Files", 0);
	LGitProgressStart(ctx, hwnd, TRUE);
	if (git_checkout_index(ctx->repo, NULL, &co_opts) != 0) {
		LGitProgressDeinit(ctx);
		LGitLibraryError(hwnd, "LGitCheckoutHead git_checkout_head");
		return SCC_E_NONSPECIFICERROR;
	}

	LGitProgressDeinit(ctx);
	return SCC_OK;
}

/**
 * This checkout emulates the SCC API semantics (list of files, use HEAD)
 */
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
	LGitContext *ctx = (LGitContext*)context;

	/*
	 * Flags mean files under a directory or recursive directory. This handles
	 * the latter; the former could be done with adjustments in pathspec?
	 * (That is, SCC_GET_ALL or SCC_GET_RECURSIVE)
	 */
	LGitLog("  flags %x", dwFlags);
	LGitLog("  files %d", nFiles);

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
		LGitPopCheckout(ctx, path);
	}
	/*
	for (i = 0; i < path_count; i++) {
		LGitLog("    In list, %s\n", path);
	}
	*/
	git_strarray strpaths;
	strpaths.strings = paths;
	strpaths.count = path_count;

	/* XXX: Use opts to check if we should checkout from idx or head */
	SCCRTN ret = LGitCheckoutHead(ctx, hWnd, &strpaths);
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
	LGitLog("**SccUncheckout** Context=%p\n", context);
	return LGitCheckoutInternal(context, hWnd, nFiles, lpFileNames, dwFlags, pvOptions);
}

/**
 * Either:
 * - Replaces files with those from HEAD.
 * - Fetches/pulls. Will prompt and apply to all files.
 */
SCCRTN SccGet (LPVOID context, 
			   HWND hWnd, 
			   LONG nFiles, 
			   LPCSTR* lpFileNames, 
			   LONG dwFlags,
			   LPCMDOPTS pvOptions)
{
	LGitLog("**SccGet** Context=%p\n", context);
	LGitLog("  options %p", pvOptions);
	LGitGetOpts *getOpts = (LGitGetOpts*)pvOptions;
	if (pvOptions != NULL && getOpts->pull) {
		LGitContext *ctx = (LGitContext*)context;
		return LGitPullDialog(ctx, hWnd);
	} else {
		return LGitCheckoutInternal(context, hWnd, nFiles, lpFileNames, dwFlags, pvOptions);
	}
}

static void LGitUnmarkReadOnly(LPCSTR fileName)
{
	DWORD attr;
	attr = GetFileAttributes(fileName);
	if (attr == INVALID_FILE_ATTRIBUTES) {
		return;
	}
	attr &= ~FILE_ATTRIBUTE_READONLY;
	SetFileAttributes(fileName, attr);
}

void LGitPushCheckout(LGitContext *ctx, const char *fileName)
{
	ctx->checkouts->insert(std::string(fileName));
}

BOOL LGitPopCheckout(LGitContext *ctx, const char *fileName)
{
	std::string name = std::string(fileName);
	BOOL exists = ctx->checkouts->count(name);
	if (exists) {
		ctx->checkouts->erase(name);
	}
	return exists;
}

BOOL LGitIsCheckout(LGitContext *ctx, const char *fileName)
{
	return ctx->checkouts->count(std::string(fileName));
}

SCCRTN SccCheckout (LPVOID context, 
					HWND hWnd, 
					LONG nFiles, 
					LPCSTR* lpFileNames, 
					LPCSTR lpComment, 
					LONG dwFlags,
					LPCMDOPTS pvOptions)
{
	LGitContext *ctx = (LGitContext*)context;
	int i;
	LGitLog("**SccCheckout** Context=%p\n", context);
	LGitLog("  flags %x", dwFlags);
	LGitLog("  files %d", nFiles);
	/*
	 * VB6 and especially VS.NET will mark files read-only after checkin or
	 * uncheckout. VS.NET will *also* try to check the read-only status of
	 * files, so what we'll do is just temporarily add them to a list.
	 */
	const char *raw_path;
	for (i = 0; i < nFiles; i++) {
		char path[1024];
		raw_path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", lpFileNames[i]);
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		strlcpy(path, raw_path, 1024);
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog("    Checking out %s\n", path);
		LGitPushCheckout(ctx, path);
		LGitUnmarkReadOnly(lpFileNames[i]);
	}
	return SCC_OK;
}