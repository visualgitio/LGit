/*
 * Commit, remove, and other index operations
 */

#include "stdafx.h"

static void AppendVerbTrailer(char *buf, const char *verb, size_t bufsz)
{
	if (verb == NULL) {
		return;
	}
	char verb_trailer[128];
	if (strlen(buf) > 0 && buf[strlen(buf) - 1] != '\n') {
		strlcat(buf, "\n", bufsz);
	}
	_snprintf(verb_trailer, 128, "\nX-SccAction: %s\n", verb);
	strlcat(buf, verb_trailer, bufsz);
}

static char *PrettifySccMessage(HWND hwnd, LGitContext *ctx, const char *message, const char *verb)
{
	/* NULL messages can't be reasoned with */
	if (message == NULL) {
		char null_msg[1024];
		strlcat(null_msg, "(No commit message specified.)\n", 1024);
		return strdup(null_msg);
	}
	/*
	 * The comment length (see SccInitialize) should be more than what SCC
	 * declares because of conversions.
	 * XXX: Heap allocate?
	 */
#define COMMITMSGSZ 8192
	char to_clean[COMMITMSGSZ];
	wchar_t to_clean_utf16[COMMITMSGSZ];
	MultiByteToWideChar(CP_ACP, 0, message, -1, to_clean_utf16, COMMITMSGSZ);
	LGitWideToUtf8(to_clean_utf16, to_clean, COMMITMSGSZ);
	/* Append the action that we're taking */
	LGitLog(" ! Verb is %s\n", verb);
	AppendVerbTrailer(to_clean, verb, COMMITMSGSZ);
	/*
	 * If we can use the prettified commit, do so, otherwise use the original.
	 * git prefers prettified because it'll make sure there's all the fixings
	 * like a trailing newline and no comments. It'll also remove CRs.
	 */
	git_buf prettified_message = {0};
	char *comment, *ret;
	if (git_message_prettify(&prettified_message, to_clean, 1, '#') != 0) {
		LGitLog(" ! Failed to prettify commit, using original text\n");
		comment = to_clean;
	} else {
		comment = prettified_message.ptr;
	}

	ret = strdup(comment);
	git_buf_dispose(&prettified_message);
	return ret;
}

static SCCRTN LGitCommitIndexCommon(HWND hWnd,
									LGitContext *ctx,
									git_index *index,
									/* locals for other funcs */
									git_oid *tree_oid,
									git_tree **tree)
{
	SCCRTN ret = SCC_OK;

	if (git_index_write_tree(tree_oid, index) != 0) {
		LGitLibraryError(hWnd, "Commit (writing tree from index)");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	if (git_index_write(index) != 0) {
		LGitLibraryError(hWnd, "Commit (writing index)");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	if (git_tree_lookup(tree, ctx->repo, tree_oid) != 0) {
		LGitLibraryError(hWnd, "Commit (tree lookup)");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
fin:;
	return ret;
}

SCCRTN LGitCommitIndex(HWND hWnd,
					   LGitContext *ctx,
					   git_index *index,
					   LPCSTR comment,
					   git_signature *author,
					   git_signature *committer)
{
	git_oid commit_oid,tree_oid;
	git_tree *tree = NULL;
	git_object *parent = NULL;
	git_reference *ref = NULL;

	SCCRTN ret = SCC_OK;
	/* Harmless if it fails, it means first commit. Need to free after. */
	if (git_revparse_ext(&parent, &ref, ctx->repo, "HEAD") != 0) {
		LGitLog(" ! Failed to get HEAD, likely the first commit\n");
	}

	ret = LGitCommitIndexCommon(hWnd, ctx, index, &tree_oid, &tree);
	if (ret != SCC_OK) {
		LGitLog("!! Failure from shared code\n");
		goto fin;
	}

	if (git_commit_create_v(
			&commit_oid,
			ctx->repo,
			"HEAD",
			author,
			committer,
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
	git_repository_message_remove(ctx->repo);
	LGitLog(" ! Made commit\n");
fin:
	if (parent != NULL) {
		git_object_free(parent);
	}
	if (ref != NULL) {
		git_reference_free(ref);
	}
	if (tree != NULL) {
		git_tree_free(tree);
	}
	return ret;
}

SCCRTN LGitCommitIndexAmendHead(HWND hWnd,
								LGitContext *ctx,
								git_index *index,
								LPCSTR comment,
								git_signature *author,
								git_signature *committer)
{

	git_oid commit_oid,tree_oid;
	git_tree *tree = NULL;
	git_commit *parent_commit = NULL;
	git_object *parent = NULL;
	git_reference *ref = NULL;

	SCCRTN ret = SCC_OK;

	/* Unlike the other function, we MUST need this to be a commit. */
	if (git_revparse_ext(&parent, &ref, ctx->repo, "HEAD")) {
		LGitLibraryError(hWnd, "Commit (fetching HEAD)");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	if (git_object_peel((git_object**)&parent_commit, parent, GIT_OBJECT_COMMIT) != 0) {
		LGitLibraryError(hWnd, "Commit (fetching HEAD as commit)");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}

	ret = LGitCommitIndexCommon(hWnd, ctx, index, &tree_oid, &tree);
	if (ret != SCC_OK) {
		LGitLog("!! Failure from shared code\n");
		goto fin;
	}

	if (git_commit_amend(
			&commit_oid,
			parent_commit,
			"HEAD",
			author == NULL ? git_commit_author(parent_commit) : author,
			committer == NULL ? git_commit_committer(parent_commit) : committer,
			NULL,
			comment,
			tree)) {
		LGitLibraryError(hWnd, "Commit (create)");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	/* Cleanup for any i.e. merging operations. */
	git_repository_state_cleanup(ctx->repo);
	git_repository_message_remove(ctx->repo);
	LGitLog(" ! Made commit\n");
fin:
	if (parent_commit != NULL) {
		git_commit_free(parent_commit);
	}
	if (parent != NULL) {
		git_object_free(parent);
	}
	if (ref != NULL) {
		git_reference_free(ref);
	}
	if (tree != NULL) {
		git_tree_free(tree);
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
	git_signature *signature = NULL;
	char *commit_message = NULL;
	LGitCommitOpts *commitOpts;
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
		char path[2048];
		LGitAnsiToUtf8(lpFileNames[i], path, 2048);
		const char *raw_path = LGitStripBasePath(ctx, path);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", path);
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog("    %s\n", raw_path);
		if (git_index_add_bypath(index, raw_path) != 0) {
			LGitLibraryError(hWnd, raw_path);
		}
		LGitPopCheckout(ctx, raw_path);
	}
	if (LGitGetDefaultSignature(hWnd, ctx, &signature) != SCC_OK) {
		goto fin;
	}
	commit_message = PrettifySccMessage(hWnd, ctx, lpComment, "Checkin");
	inner_ret = LGitCommitIndex(hWnd, ctx, index, commit_message, signature, signature);
	commitOpts = (LGitCommitOpts*)pvOptions;
	if (pvOptions != NULL && commitOpts->push && inner_ret == SCC_OK) {
		inner_ret = LGitPushDialog(ctx, hWnd);
	}
fin:
	free(commit_message);
	git_index_free(index);
	git_signature_free(signature);
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
	git_signature *signature = NULL;
	char *commit_message = NULL;
	LGitCommitOpts *commitOpts;
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
		char path[2048];
		LGitAnsiToUtf8(lpFileNames[i], path, 2048);
		const char *raw_path = LGitStripBasePath(ctx, path);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", path);
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog("    %s\n", raw_path);
		if (git_index_add_bypath(index, raw_path) != 0) {
			LGitLibraryError(hWnd, raw_path);
		}
		LGitPopCheckout(ctx, raw_path);
	}
	if (LGitGetDefaultSignature(hWnd, ctx, &signature) != SCC_OK) {
		goto fin;
	}
	commit_message = PrettifySccMessage(hWnd, ctx, lpComment, "Add");
	inner_ret = LGitCommitIndex(hWnd, ctx, index, commit_message, signature, signature);
	commitOpts = (LGitCommitOpts*)pvOptions;
	if (pvOptions != NULL && commitOpts->push && inner_ret == SCC_OK) {
		inner_ret = LGitPushDialog(ctx, hWnd);
	}
fin:
	free(commit_message);
	git_index_free(index);
	git_signature_free(signature);
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
	git_signature *signature = NULL;
	char *commit_message = NULL;
	LGitCommitOpts *commitOpts;
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
		char path[2048];
		LGitAnsiToUtf8(lpFileNames[i], path, 2048);
		const char *raw_path = LGitStripBasePath(ctx, path);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", path);
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog("    Removing %s\n", raw_path);
		if (git_index_remove_bypath(index, raw_path) != 0) {
			LGitLibraryError(hWnd, raw_path);
		}
		LGitPopCheckout(ctx, raw_path);
	}
	if (LGitGetDefaultSignature(hWnd, ctx, &signature) != SCC_OK) {
		goto fin;
	}
	commit_message = PrettifySccMessage(hWnd, ctx, lpComment, "Remove");
	inner_ret = LGitCommitIndex(hWnd, ctx, index, commit_message, signature, signature);
	commitOpts = (LGitCommitOpts*)pvOptions;
	if (pvOptions != NULL && commitOpts->push && inner_ret == SCC_OK) {
		inner_ret = LGitPushDialog(ctx, hWnd);
	}
fin:
	free(commit_message);
	git_index_free(index);
	git_signature_free(signature);
	return inner_ret;
}

SCCRTN SccRename (LPVOID context, 
				  HWND hWnd, 
				  LPCSTR lpFileName,
				  LPCSTR lpNewName)
{
	LGitContext *ctx = (LGitContext*)context;
	git_index *index;
	git_signature *signature = NULL;
	SCCRTN inner_ret;
	const char *o_stripped_path, *n_stripped_path;
	char o_path[1024], n_path[1024], comment[COMMITMSGSZ];

	/* OLD */
	LGitLog("**SccRename** Context=%p\n", context);
	LGitAnsiToUtf8(lpFileName, o_path, 1024);
	o_stripped_path = LGitStripBasePath(ctx, o_path);
	if (o_stripped_path == NULL) {
		LGitLog("    Couldn't get base path for %s\n", lpFileName);
		inner_ret = SCC_E_OPNOTPERFORMED;
		goto fail;
	}
	LGitTranslateStringChars(o_path, '\\', '/');
	LGitLog("  Old %s\n", o_stripped_path);
	/* NEW */
	LGitAnsiToUtf8(lpNewName, n_path, 1024);
	n_stripped_path = LGitStripBasePath(ctx, n_path);
	if (n_stripped_path == NULL) {
		LGitLog("    Couldn't get base path for %s\n", lpNewName);
		inner_ret = SCC_E_OPNOTPERFORMED;
		goto fail;
	}
	LGitTranslateStringChars(n_path, '\\', '/');
	LGitLog("  New %s\n", n_stripped_path);
	/* Take a slightly roundabout method */
	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "Remove (getting index)");
		inner_ret = SCC_E_NONSPECIFICERROR;
		goto fail;
	}
	const git_index_entry *old_entry;
	git_index_entry new_entry;
	old_entry = git_index_get_bypath(index, o_stripped_path, 0);
	memcpy(&new_entry, old_entry, sizeof(git_index_entry));
	new_entry.path = n_stripped_path;
	if (git_index_add(index, &new_entry) != 0) {
		LGitLibraryError(hWnd, "SccRename git_index_add");
		inner_ret = SCC_E_NONSPECIFICERROR;
		goto fail;
	}
	if (git_index_remove_bypath(index, o_stripped_path) != 0) {
		LGitLibraryError(hWnd, "SccRename git_index_remove_bypath");
		inner_ret = SCC_E_NONSPECIFICERROR;
		goto fail;
	}
	_snprintf(comment, COMMITMSGSZ, "Rename %s to %s\n\nX-SccAction: Rename\n", o_path, n_path);
	if (LGitGetDefaultSignature(hWnd, ctx, &signature) != SCC_OK) {
		goto fail;
	}
	inner_ret = LGitCommitIndex(hWnd, ctx, index, comment, signature, signature);
	LGitPopCheckout(ctx, o_stripped_path);
fail:
	git_index_free(index);
	git_signature_free(signature);
	return inner_ret;
}