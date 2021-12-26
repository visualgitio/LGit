/*
 * Cherry pick commits.
 */

#include <stdafx.h>

/**
 * Cherry-picks into the current index and WT.
 */
SCCRTN LGitCherryPickCommit(LGitContext *ctx,
							HWND hwnd,
							const git_oid *commit_oid)
{
	LGitLog("**LGitCherryPickCommit** Context=%p\n", ctx);
	LGitLog("  oid %s\n", git_oid_tostr_s(commit_oid));
	SCCRTN ret = SCC_E_NONSPECIFICERROR;
	git_cherrypick_options cherry_opts;
	git_cherrypick_options_init(&cherry_opts, GIT_CHERRYPICK_OPTIONS_VERSION);
	LGitInitCheckoutProgressCallback(ctx, &cherry_opts.checkout_opts);
	LGitInitCheckoutNotifyCallbacks(ctx, hwnd, &cherry_opts.checkout_opts);
	git_commit *commit = NULL;
	if (git_commit_lookup(&commit, ctx->repo, commit_oid) != 0) {
		LGitLibraryError(hwnd, "git_commit_lookup");
		goto err;
	}
	LGitProgressInit(ctx, "Cherry Picking Commit", 0);
	LGitProgressStart(ctx, hwnd, TRUE);
	if (git_cherrypick(ctx->repo, commit, &cherry_opts) != 0) {
		LGitProgressDeinit(ctx);
		LGitLibraryError(hwnd, "git_cherrypick");
		goto err;
	}
	LGitProgressDeinit(ctx);
	ret = SCC_OK;
err:
	LGitFinishCheckoutNotify(ctx, hwnd, &cherry_opts.checkout_opts);
	git_commit_free(commit);
	return ret;
}