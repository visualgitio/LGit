/*
 * Commit, remove, and other index operations
 */

#include "stdafx.h"
#include "LGit.h"

static SCCRTN LGitCommitIndex(HWND hWnd,
							  LGitContext *ctx,
							  git_index *index,
							  LPCSTR lpComment)
{
	git_oid commit_oid,tree_oid;
	git_tree *tree;
	git_object *parent = NULL;
	git_reference *ref = NULL;
	git_signature *signature;

	const char *comment;
	git_buf prettified_message = {0};
	/*
	 * If we can use the prettified commit, do so, otherwise use the original.
	 * git prefers prettified because it'll make sure there's all the fixings
	 * like a trailing newline and no comments.
	 */
	if (git_message_prettify(&prettified_message, lpComment, 1, '#') != 0) {
		LGitLog(" ! Failed to prettify commit, using original text\n");
		comment = lpComment;
	} else {
		comment = prettified_message.ptr;
	}

	// Harmless if it fails, it means first commit
	if (git_revparse_ext(&parent, &ref, ctx->repo, "HEAD") != 0) {
		LGitLog(" ! Failed to get HEAD, likely the first commit\n");
	}

	if (git_index_write_tree(&tree_oid, index) != 0) {
		LGitLibraryError(hWnd, "Commit (writing tree from index)");
		git_buf_dispose(&prettified_message);
		return SCC_E_NONSPECIFICERROR;
	}
	if (git_index_write(index) != 0) {
		LGitLibraryError(hWnd, "Commit (writing tree from index)");
		git_buf_dispose(&prettified_message);
		return SCC_E_NONSPECIFICERROR;
	}
	if (git_tree_lookup(&tree, ctx->repo, &tree_oid) != 0) {
		LGitLibraryError(hWnd, "Commit (tree lookup)");
		git_buf_dispose(&prettified_message);
		return SCC_E_NONSPECIFICERROR;
	}
	if (git_signature_default(&signature, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "Commit (signature)");
		git_buf_dispose(&prettified_message);
		git_tree_free(tree);
		return SCC_E_NONSPECIFICERROR;
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
		git_buf_dispose(&prettified_message);
		git_tree_free(tree);
		git_signature_free(signature);
		return SCC_E_NONSPECIFICERROR;
	}
	git_buf_dispose(&prettified_message);
	git_tree_free(tree);
	git_signature_free(signature);
	return SCC_OK;
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

	LGitLog("**SccCheckin**\n");
	LGitLog("  comment %s", lpComment);
	LGitLog("  flags %x", dwFlags);
	LGitLog("  files %d", nFiles);

	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "Checkin (getting index)");
		return SCC_E_NONSPECIFICERROR;
	}

	for (i = 0; i < nFiles; i++) {
		const char *path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (path == NULL) {
			LGitLog("    Error stripping %s\n", lpFileNames[i]);
			continue;
		}
		LGitLog("    Adding %s\n", lpFileNames[i]);
		if (git_index_add_bypath(index, path) != 0) {
			LGitLibraryError(hWnd, path);
		}
	}
	inner_ret = LGitCommitIndex(hWnd, ctx, index, lpComment);
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

	LGitLog("**SccAdd**\n");
	LGitLog("  comment %s", lpComment);
	LGitLog("  files %d", nFiles);

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
		const char *path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (path == NULL) {
			LGitLog("    Error stripping %s\n", lpFileNames[i]);
			continue;
		}
		LGitLog("    Adding %s\n", lpFileNames[i]);
		if (git_index_add_bypath(index, path) != 0) {
			LGitLibraryError(hWnd, path);
		}
	}
	inner_ret = LGitCommitIndex(hWnd, ctx, index, lpComment);
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

	LGitLog("**SccRemove**\n");
	LGitLog("  comment %s", lpComment);
	LGitLog("  flags %x", dwFlags);
	LGitLog("  files %d", nFiles);

	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "Remove (getting index)");
		return SCC_E_NONSPECIFICERROR;
	}

	for (i = 0; i < nFiles; i++) {
		const char *path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (path == NULL) {
			LGitLog("    Error stripping %s\n", lpFileNames[i]);
			continue;
		}
		LGitLog("    Adding %s\n", lpFileNames[i]);
		if (git_index_remove_bypath(index, path) != 0) {
			LGitLibraryError(hWnd, path);
		}
	}
	inner_ret = LGitCommitIndex(hWnd, ctx, index, lpComment);
	git_index_free(index);
	return inner_ret;
}

SCCRTN SccRename (LPVOID context, 
				  HWND hWnd, 
				  LPCSTR lpFileName,
				  LPCSTR lpNewName)
{
	LGitLog("**SccRename** %s -> %s", lpFileName, lpNewName);
	// https://github.com/libgit2/libgit2/blob/508361401fbb5d87118045eaeae3356a729131aa/tests/index/rename.c#L4
	return SCC_E_OPNOTSUPPORTED;
}