/*
 * History of files.
 */

#include "stdafx.h"
#include "LGit.h"

static LVCOLUMN author_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 175, "Author"
};
static LVCOLUMN authored_when_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 125, "Authored At"
};
static LVCOLUMN comment_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 200, "Comment"
};
static LVCOLUMN oid_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 75, "Object ID"
};

/* 
 * All these variables are a pain to carry indiviually, so we pack a pointer
 * to this in LPARAM. The DialogBoxParam call is anchored to the source of
 * all of this, so they won't expire lifetime-wise past the dialog.
 */
typedef struct _LGitHistoryDialogParams {
	LGitContext *ctx;
	git_revwalk *walker;
	git_pathspec *ps;
	git_diff_options *diffopts;
	
	const char **paths;
	int path_count;
} LGitHistoryDialogParams;

static void SetFileCountTitleBar(HWND hwnd, LGitHistoryDialogParams *params)
{
	char title[256];
	/* XXX: Load string resources */
	if (params->path_count == 1) {
		_snprintf(title, 256, "Commit History for %s", params->paths[0]);
	} else {
		_snprintf(title, 256, "Commit History for %d files", params->path_count);
	}
	SetWindowText(hwnd, title);
}

static void InitializeHistoryListView(HWND hwnd)
{
	HWND lv;

	lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);

	ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT
		| LVS_EX_HEADERDRAGDROP
		| LVS_EX_LABELTIP);

	ListView_InsertColumn(lv, 0, &oid_column);
	ListView_InsertColumn(lv, 1, &author_column);
	ListView_InsertColumn(lv, 2, &authored_when_column);
	ListView_InsertColumn(lv, 3, &comment_column);
	/*
	 * XXX: Wonder if maybe callbacks are the way to go:
	 * https://docs.microsoft.com/en-us/windows/win32/controls/add-list-view-items-and-subitems
	 */
}

/** Helper to find how many files in a commit changed from its nth parent. */
static int match_with_parent(HWND hwnd, git_commit *commit, int i, git_diff_options *opts)
{
	git_commit *parent;
	git_tree *a, *b;
	git_diff *diff;
	int ndeltas;

	if (git_commit_parent(&parent, commit, (size_t)i) != 0) {
		LGitLibraryError(hwnd, "match_with_parent git_commit_parent");
		return -1;
	}
	if (git_commit_tree(&a, parent) != 0) {
		LGitLibraryError(hwnd, "match_with_parent git_commit_tree A");
		git_commit_free(parent);
		return -1;
	}
	if (git_commit_tree(&b, commit) != 0) {
		LGitLibraryError(hwnd, "match_with_parent git_commit_tree B");
		git_tree_free(a);
		git_commit_free(parent);
		return -1;
	}
	if (git_diff_tree_to_tree(&diff, git_commit_owner(commit), a, b, opts) != 0) {
		LGitLibraryError(hwnd, "match_with_parent git_commit_tree B");
		git_tree_free(a);
		git_tree_free(b);
		git_commit_free(parent);
		return -1;
	}

	ndeltas = (int)git_diff_num_deltas(diff);

	git_diff_free(diff);
	git_tree_free(a);
	git_tree_free(b);
	git_commit_free(parent);

	return ndeltas > 0;
}

static BOOL FillHistoryListView(HWND hwnd,
								LGitHistoryDialogParams *param,
								BOOL whole_repo)
{
	HWND lv;
	git_oid oid;
	git_commit *commit = NULL;
	int parents, i;

	LGitContext *ctx = param->ctx;
	git_revwalk *walker = param->walker;
	git_pathspec *ps = param->ps;
	git_diff_options *diffopts = param->diffopts;

	lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);

	for (; !git_revwalk_next(&oid, walker); git_commit_free(commit)) {
		const git_signature *author, *committer;
		const char *message;
		char *oid_str; /* owned by library statically, do not free */
		char formatted[256];
		LVITEM lvi;

		LGitLog("!! Revwalk next\n");
		if (git_commit_lookup(&commit, ctx->repo, &oid) != 0) {
			LGitLibraryError(hwnd, "SccHistory git_commit_lookup");
			git_commit_free(commit);
			return FALSE;
		}

		parents = (int)git_commit_parentcount(commit);

		/*
		 * In examples/log.c, this assumes a pathspec is set, but we will (for
		 * now) always have at least one file in the pathspec.
		 */
		if (!whole_repo) {
			int unmatched = parents;

			if (parents == 0) {
				git_tree *tree;
				if (git_commit_tree(&tree, commit) != 0) {
					LGitLibraryError(hwnd, "SccHistory git_commit_lookup");
					git_commit_free(commit);
					return FALSE;
				}
				if (git_pathspec_match_tree(
						NULL, tree, GIT_PATHSPEC_NO_MATCH_ERROR, ps) != 0)
					unmatched = 1;
				git_tree_free(tree);
			} else if (parents == 1) {
				unmatched = match_with_parent(hwnd, commit, 0, diffopts) ? 0 : 1;
			} else {
				for (i = 0; i < parents; ++i) {
					if (match_with_parent(hwnd, commit, i, diffopts))
						unmatched--;
				}
			}

			if (unmatched > 0)
				continue;
		}

		/* Actually insert */
		oid_str = git_oid_tostr_s(&oid);
		author = git_commit_author(commit);
		committer = git_commit_committer(commit);
		message = git_commit_message(commit);

		ZeroMemory(&lvi, sizeof(LVITEM));
		lvi.mask = LVIF_TEXT;
		lvi.pszText = oid_str;
		lvi.iSubItem = 0;

		lvi.iItem = ListView_InsertItem(lv, &lvi);
		if (lvi.iItem == -1) {
			LGitLog(" ! ListView_InsertItem failed for %s\n", oid_str);
			continue;
		}
		/* now for the subitems... */
		lvi.iSubItem = 1;
		LGitFormatSignature(author, formatted, 256);
		lvi.pszText = formatted;
		ListView_SetItem(lv, &lvi);
		
		lvi.iSubItem = 2;
		LGitTimeToString(&author->when, formatted, 256);
		lvi.pszText = formatted;
		ListView_SetItem(lv, &lvi);

		lvi.iSubItem = 3;
		lvi.pszText = (char*)git_commit_summary(commit);
		ListView_SetItem(lv, &lvi);
	}
	return TRUE;
}

static BOOL CALLBACK HistoryDialogProc(HWND hwnd,
									   unsigned int iMsg,
									   WPARAM wParam,
									   LPARAM lParam)
{
	LGitHistoryDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitHistoryDialogParams*)lParam;
		SetFileCountTitleBar(hwnd, param);
		InitializeHistoryListView(hwnd);
		if (!FillHistoryListView(hwnd, param, param->path_count == 0)) {
			EndDialog(hwnd, 0);
		}
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			EndDialog(hwnd, 1);
		}
		return TRUE;
	default:
		return FALSE;
	}
}

/*
 * Note that this takes a list of files; we only should grab the commits
 * relevant for those files.
 */
SCCRTN SccHistory (LPVOID context, 
				   HWND hWnd, 
				   LONG nFiles, 
				   LPCSTR* lpFileNames, 
				   LONG dwFlags,
				   LPCMDOPTS pvOptions)
{
	git_diff_options diffopts;
	git_pathspec *ps = NULL;
	git_revwalk *walker;

	LGitHistoryDialogParams params;

	int i, path_count;
	const char **paths, *path;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccHistory** Flags %x, count %d\n", dwFlags, nFiles);

	git_diff_options_init(&diffopts, GIT_DIFF_OPTIONS_VERSION);

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

	diffopts.pathspec.strings = (char**)paths;
	diffopts.pathspec.count	  = path_count;
	if (path_count > 0) {
		if (git_pathspec_new(&ps, &diffopts.pathspec) != 0) {
			LGitLibraryError(hWnd, "SccHistory git_pathspec_new");
			free(paths);
			return SCC_E_NONSPECIFICERROR;
		}
	}

	if (git_revwalk_new(&walker, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "SccHistory git_revwalk_new");
		free(paths);
		return SCC_E_NONSPECIFICERROR;
	}
	if (git_revwalk_push_head(walker) != 0) {
		LGitLibraryError(hWnd, "SccHistory git_revwalk_push_head");
		free(paths);
		return SCC_E_NONSPECIFICERROR;
	}
	git_revwalk_sorting(walker, GIT_SORT_TIME | GIT_SORT_REVERSE);

	params.ctx = ctx;
	params.walker = walker;
	params.ps = ps;
	params.diffopts = &diffopts;
	params.paths = paths;
	params.path_count = path_count;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_COMMITHISTORY),
		hWnd,
		HistoryDialogProc,
		(LPARAM)&params)) {
	case 0:
		LGitLog(" ! Uh-oh, dialog error\n");
		return SCC_E_NONSPECIFICERROR;
	default:
		break;
	}

	git_pathspec_free(ps);
	git_revwalk_free(walker);
	free(paths);
	return SCC_OK;
}