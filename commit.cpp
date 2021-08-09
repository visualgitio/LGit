/*
 * Commit, remove, and other index operations
 */

#include "stdafx.h"

static SCCRTN LGitCommitIndex(HWND hWnd,
							  LGitContext *ctx,
							  git_index *index,
							  LPCSTR lpComment)
{
	git_oid commit_oid,tree_oid;
	git_tree *tree = NULL;
	git_object *parent = NULL;
	git_reference *ref = NULL;
	git_signature *signature = NULL;

	SCCRTN ret = SCC_OK;

	const char *comment;
	git_buf prettified_message = {0};
	/*
	 * If we can use the prettified commit, do so, otherwise use the original.
	 * git prefers prettified because it'll make sure there's all the fixings
	 * like a trailing newline and no comments.
	 */
	if (lpComment == NULL) {
		/* Null commit messages will crash libgit2 */
		comment = "(No commit message specified.)\n";
	} else if (git_message_prettify(&prettified_message, lpComment, 1, '#') != 0) {
		LGitLog(" ! Failed to prettify commit, using original text\n");
		comment = lpComment;
	} else {
		comment = prettified_message.ptr;
	}

	/* Harmless if it fails, it means first commit. Need to free after. */
	if (git_revparse_ext(&parent, &ref, ctx->repo, "HEAD") != 0) {
		LGitLog(" ! Failed to get HEAD, likely the first commit\n");
	}

	if (git_index_write_tree(&tree_oid, index) != 0) {
		LGitLibraryError(hWnd, "Commit (writing tree from index)");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	if (git_index_write(index) != 0) {
		LGitLibraryError(hWnd, "Commit (writing tree from index)");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	if (git_tree_lookup(&tree, ctx->repo, &tree_oid) != 0) {
		LGitLibraryError(hWnd, "Commit (tree lookup)");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	if (git_signature_default(&signature, ctx->repo) != 0) {
		/* The git config is empty, so prompt for a signature */
		char name[128], mail[128];
		if (LGitSignatureDialog(ctx, hWnd, name, 128, mail, 128)) {
			if (git_signature_now(&signature, name, mail) != 0) {
				/* You tried */
				LGitLibraryError(hWnd, "Commit (new signature)");
				ret = SCC_E_NONSPECIFICERROR;
				goto fin;
			}
		} else {
			/* You tried */
			LGitLibraryError(hWnd, "Commit (existing signature)");
			ret = SCC_E_NONSPECIFICERROR;
			goto fin;
		}
	}
	if (git_commit_create_v(
			&commit_oid,
			ctx->repo,
			"HEAD",
			signature,
			signature,
			NULL,
			comment,
			tree,
			parent ? 1 : 0, parent)) {
		LGitLibraryError(hWnd, "Commit (create)");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	/* Cleanup for any i.e. merging operations. */
	git_repository_state_cleanup(ctx->repo);
	LGitLog(" ! Made commit\n");
fin:
	git_buf_dispose(&prettified_message);
	if (parent != NULL) {
		git_object_free(parent);
	}
	if (ref != NULL) {
		git_reference_free(ref);
	}
	if (tree != NULL) {
		git_tree_free(tree);
	}
	if (signature != NULL) {
		git_signature_free(signature);
	}
	return ret;
}

/**
 * Conceptually, checking in is close to a "git add" followed by "git commit".
 * We don't care if the user checked "check out again" afterwards, because it
 * must be checked out due to how git works and how we model git.
 */
SCCRTN SccCheckin (LPVOID context, 
				   HWND hWnd, 
				   LONG nFiles, 
				   LPCSTR* lpFileNames, 
				   LPCSTR lpComment, 
				   LONG dwFlags,
				   LPCMDOPTS pvOptions)
{
	git_index *index;
	SCCRTN inner_ret;
	int i;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccCheckin** Context=%p\n", context);
	LGitLog("  comment %s", lpComment);
	LGitLog("  flags %x", dwFlags);
	LGitLog("  files %d", nFiles);
	LGitLog("  opts  %p", pvOptions);

	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "Checkin (getting index)");
		return SCC_E_NONSPECIFICERROR;
	}

	for (i = 0; i < nFiles; i++) {
		const char *raw_path;
		char path[1024];
		raw_path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", lpFileNames[i]);
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		strlcpy(path, raw_path, 1024);
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog("    Adding %s\n", path);
		if (git_index_add_bypath(index, path) != 0) {
			LGitLibraryError(hWnd, path);
		}
		LGitPopCheckout(ctx, path);
	}
	inner_ret = LGitCommitIndex(hWnd, ctx, index, lpComment);
	LGitCommitOpts *commitOpts = (LGitCommitOpts*)pvOptions;
	if (pvOptions != NULL && commitOpts->push && inner_ret == SCC_OK) {
		inner_ret = LGitPushDialog(ctx, hWnd);
	}
	git_index_free(index);
	return inner_ret;
}

/**
 * Like checkin, but for files that the IDE doesn't think are in SCC.
 */
SCCRTN SccAdd (LPVOID context, 
			   HWND hWnd, 
			   LONG nFiles, 
			   LPCSTR* lpFileNames, 
			   LPCSTR lpComment, 
			   LONG * pdwFlags,
			   LPCMDOPTS pvOptions)
{
	git_index *index;
	SCCRTN inner_ret;
	int i;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccAdd** Context=%p\n", context);
	LGitLog("  comment %s", lpComment);
	LGitLog("  files %d", nFiles);
	LGitLog("  opts  %p", pvOptions);

	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "Add (getting index)");
		return SCC_E_NONSPECIFICERROR;
	}

	for (i = 0; i < nFiles; i++) {
		/*
		 * Annoyingly, flags are on a per-file basis.
		 * They map to SCC_FILETYPE_xxxx; which indicate text encoding
		 * or binary, as well as a flag for "storing only the latest
		 * version" of a file. None of that maps to git, as it figures
		 * out what's binary on its own without help, AFAIK.
		 *
		 * (Maybe we could use it to convert behind the IDE's back...)
		 */
		LGitLog("    Flags: %x\n", pdwFlags[i]);
		const char *raw_path;
		char path[1024];
		raw_path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", lpFileNames[i]);
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		strlcpy(path, raw_path, 1024);
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog("    Adding %s\n", path);
		if (git_index_add_bypath(index, path) != 0) {
			LGitLibraryError(hWnd, path);
		}
		LGitPopCheckout(ctx, path);
	}
	inner_ret = LGitCommitIndex(hWnd, ctx, index, lpComment);
	LGitCommitOpts *commitOpts = (LGitCommitOpts*)pvOptions;
	if (pvOptions != NULL && commitOpts->push && inner_ret == SCC_OK) {
		inner_ret = LGitPushDialog(ctx, hWnd);
	}
	git_index_free(index);
	return inner_ret;
}

/**
 * "git rm" but the IDE will delete the file on disk. We'll remove it from the
 * index. Basically symmetrical with SccCheckin.
 */
SCCRTN SccRemove (LPVOID context, 
				  HWND hWnd, 
				  LONG nFiles, 
				  LPCSTR* lpFileNames,
				  LPCSTR lpComment,
				  LONG dwFlags,
				  LPCMDOPTS pvOptions)
{
	git_index *index;
	SCCRTN inner_ret;
	int i;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccRemove** Context=%p\n", context);
	LGitLog("  comment %s", lpComment);
	LGitLog("  flags %x", dwFlags);
	LGitLog("  files %d", nFiles);
	LGitLog("  opts  %p", pvOptions);

	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "Remove (getting index)");
		return SCC_E_NONSPECIFICERROR;
	}

	for (i = 0; i < nFiles; i++) {
		const char *raw_path;
		char path[1024];
		raw_path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", lpFileNames[i]);
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		strlcpy(path, raw_path, 1024);
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog("    Removing %s\n", path);
		if (git_index_remove_bypath(index, path) != 0) {
			LGitLibraryError(hWnd, path);
		}
		LGitPopCheckout(ctx, path);
	}
	inner_ret = LGitCommitIndex(hWnd, ctx, index, lpComment);
	LGitCommitOpts *commitOpts = (LGitCommitOpts*)pvOptions;
	if (pvOptions != NULL && commitOpts->push && inner_ret == SCC_OK) {
		inner_ret = LGitPushDialog(ctx, hWnd);
	}
	git_index_free(index);
	return inner_ret;
}

SCCRTN SccRename (LPVOID context, 
				  HWND hWnd, 
				  LPCSTR lpFileName,
				  LPCSTR lpNewName)
{
	LGitContext *ctx = (LGitContext*)context;
	git_index *index;
	SCCRTN inner_ret;
	const char *o_raw_path, *n_raw_path;
	char o_path[1024], n_path[1024], comment[1024];

	LGitLog("**SccRename** Context=%p\n", context);
	o_raw_path = LGitStripBasePath(ctx, lpFileName);
	if (o_raw_path == NULL) {
		LGitLog("    Couldn't get base path for %s\n", lpFileName);
		return SCC_E_OPNOTPERFORMED;
	}
	strlcpy(o_path, o_raw_path, 1024);
	LGitTranslateStringChars(o_path, '\\', '/');
	LGitLog("  Old %s\n", o_path);
	n_raw_path = LGitStripBasePath(ctx, lpNewName);
	if (n_raw_path == NULL) {
		LGitLog("    Couldn't get base path for %s\n", lpNewName);
		return SCC_E_OPNOTPERFORMED;
	}
	strlcpy(n_path, n_raw_path, 1024);
	LGitTranslateStringChars(n_path, '\\', '/');
	LGitLog("  New %s\n", n_path);
	/* Take a slightly roundabout method */
	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "Remove (getting index)");
		return SCC_E_NONSPECIFICERROR;
	}
	const git_index_entry *old_entry;
	git_index_entry new_entry;
	old_entry = git_index_get_bypath(index, o_path, 0);
	memcpy(&new_entry, old_entry, sizeof(git_index_entry));
	new_entry.path = n_path;
	if (git_index_add(index, &new_entry) != 0) {
		LGitLibraryError(hWnd, "SccRename git_index_add");
		inner_ret = SCC_E_NONSPECIFICERROR;
		goto fail;
	}
	if (git_index_remove_bypath(index, o_path) != 0) {
		LGitLibraryError(hWnd, "SccRename git_index_remove_bypath");
		inner_ret = SCC_E_NONSPECIFICERROR;
		goto fail;
	}
	/* XXX: Add a trailer? */
	_snprintf(comment, 1024, "Rename %s to %s\n", o_path, n_path);
	inner_ret = LGitCommitIndex(hWnd, ctx, index, comment);
	LGitPopCheckout(ctx, o_path);
fail:
	git_index_free(index);
	return inner_ret;
}