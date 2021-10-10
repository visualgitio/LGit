/*
 * Reverts (and resets?) commits.
 */

 #include <stdafx.h>

SCCRTN LGitRevertCommit(LGitContext *ctx,
						HWND hwnd,
						const git_oid *commit_oid)
{
	LGitLog("**LGitRevertCommit** Context=%p\n", ctx);
	LGitLog("  oid %s\n", git_oid_tostr_s(commit_oid));
	SCCRTN ret = SCC_E_NONSPECIFICERROR;
	/*
	 * This is tricky, but we're using the easy git_revert API instead of the
	 * more intricate git_revert_commit API. It assumes HEAD (what we want)
	 * and will do a checkout after (also what we probably want). We still,
	 * however, need to make a commit after. This might include other staged
	 * stuff from the index, but the complex case means we'd have to do a
	 * checkout after, though we'd have flexibility from the output index.
	 */
	git_revert_options revert_opts;
	git_revert_options_init(&revert_opts, GIT_REVERT_OPTIONS_VERSION);
	LGitInitCheckoutProgressCallback(ctx, &revert_opts.checkout_opts);
	git_commit *commit = NULL;
	git_index *index = NULL;
	if (git_commit_lookup(&commit, ctx->repo, commit_oid) != 0) {
		LGitLibraryError(hwnd, "git_commit_lookup");
		goto err;
	}
	LGitProgressInit(ctx, "Reverting Commit", 0);
	LGitProgressStart(ctx, hwnd, TRUE);
	if (git_revert(ctx->repo, commit, &revert_opts) != 0) {
		LGitProgressDeinit(ctx);
		LGitLibraryError(hwnd, "git_revert");
		goto err;
	}
	LGitProgressDeinit(ctx);
	/* We now need to commit our changes to the index. Revert mutates repo. */
	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hwnd, "git_repository_index");
		goto err;
	}
	if (LGitCreateCommitDialog(ctx, hwnd, FALSE, NULL, index) != SCC_OK) {
		/* it'll make the dialog for us */
		goto err;
	}
	ret = SCC_OK;
err:
	/* The message is provided by the merge message, so remove it. */
	git_repository_message_remove(ctx->repo);
	if (index != NULL) {
		git_index_free(index);
	}
	if (commit != NULL) {
		git_commit_free(commit);
	}
	return ret;
}

SCCRTN LGitResetToCommit(LGitContext *ctx,
						 HWND hwnd,
						 const git_oid *commit_oid,
						 BOOL hard)
{
	LGitLog("**LGitResetToCommit** Context=%p\n", ctx);
	LGitLog("  oid %s\n", git_oid_tostr_s(commit_oid));
	LGitLog("  hard? %d\n", hard);
	SCCRTN ret = SCC_E_NONSPECIFICERROR;
	git_reset_t reset_type = hard ? GIT_RESET_HARD : GIT_RESET_SOFT;
	git_checkout_options co_opts;
	git_checkout_options_init(&co_opts, GIT_CHECKOUT_OPTIONS_VERSION);
	LGitInitCheckoutProgressCallback(ctx, &co_opts);
	if (hard) {
		/* it only makes sense, but hard is the only time it checks out anyways */
		co_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	}
	git_commit *commit = NULL;
	if (git_commit_lookup(&commit, ctx->repo, commit_oid) != 0) {
		LGitLibraryError(hwnd, "git_commit_lookup");
		goto err;
	}
	LGitProgressInit(ctx, "Reseting to Commit", 0);
	LGitProgressStart(ctx, hwnd, TRUE);
	/* it gets peeled to a commit regardless */
	if (git_reset(ctx->repo, (const git_object*)commit, reset_type, &co_opts) != 0) {
		LGitProgressDeinit(ctx);
		LGitLibraryError(hwnd, "git_reset");
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