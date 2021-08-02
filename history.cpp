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
	
	char **paths;
	int path_count;

	/* window sundry */
	HMENU menu;
} LGitHistoryDialogParams;

static void InitializeHistoryWindow(HWND hwnd, LGitHistoryDialogParams *params)
{
	SetMenu(hwnd, params->menu);
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
		LGitLibraryError(hwnd, "match_with_parent git_diff_tree_to_tree");
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
	int parents, i, index;

	LGitContext *ctx = param->ctx;
	git_revwalk *walker = param->walker;
	git_pathspec *ps = param->ps;
	git_diff_options *diffopts = param->diffopts;

	lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);

	/*
	 * We can only get the revision count by walking it like we're doing now,
	 * so it's unquantifiable.
	 */
	LGitProgressInit(param->ctx, "Walking History", 0);
	LGitProgressStart(param->ctx, hwnd, FALSE);
	for (index = 0; !git_revwalk_next(&oid, walker); git_commit_free(commit)) {
		const git_signature *author, *committer;
		const char *message;
		char *oid_str; /* owned by library statically, do not free */
		char formatted[256];
		LVITEM lvi;

		if (LGitProgressCancelled(param->ctx)) {
			/* We'll work with what we have. */
			break;
		}

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

		/* Let's inform the dialog. May spam TextOut tho */
		LGitProgressText(param->ctx, oid_str, 1);

		ZeroMemory(&lvi, sizeof(LVITEM));
		lvi.mask = LVIF_TEXT;
		lvi.pszText = oid_str;
		lvi.iItem = index++;
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
	LGitProgressDeinit(param->ctx);
	/* Recalculate after adding because of scroll bars */
	ListView_SetColumnWidth(lv, 3, LVSCW_AUTOSIZE_USEHEADER);
	return TRUE;
}

static void ShowSelectedCommitDiff(HWND hwnd, LGitHistoryDialogParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);
	if (lv == NULL) {
		return;
	}
	int selected = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
	if (selected == -1) {
		return;
	}
	/* get the OID as a string then convert */
	char oid_s[80];
	ListView_GetItemText(lv, selected, 0, oid_s, 80);
	git_oid oid;
	if (git_oid_fromstr(&oid, oid_s) != 0) {
		LGitLibraryError(hwnd, "git_oid_fromstr");
		return;
	}
	/* now try to find the parent to make a diff from */
	git_commit *commit = NULL;
	if (git_commit_lookup(&commit, params->ctx->repo, &oid) != 0) {
		LGitLibraryError(hwnd, "git_commit_lookup");
		return;
	}
	LGitCommitToParentDiff(params->ctx, hwnd, commit, params->diffopts);
	if (commit != NULL) {
		git_commit_free(commit);
	}
}

static void ShowSelectedCommitInfo(HWND hwnd, LGitHistoryDialogParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);
	if (lv == NULL) {
		return;
	}
	int selected = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
	if (selected == -1) {
		return;
	}
	/* get the OID as a string then convert */
	char oid_s[80];
	ListView_GetItemText(lv, selected, 0, oid_s, 80);
	git_oid oid;
	if (git_oid_fromstr(&oid, oid_s) != 0) {
		LGitLibraryError(hwnd, "git_oid_fromstr");
		return;
	}
	/* now try to find the parent to make a diff from */
	git_commit *commit = NULL;
	if (git_commit_lookup(&commit, params->ctx->repo, &oid) != 0) {
		LGitLibraryError(hwnd, "git_commit_lookup");
		return;
	}
	LGitViewCommitInfo(params->ctx, hwnd, commit);
}

static void UpdateHistoryMenu(HWND hwnd, LGitHistoryDialogParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);
	UINT selected = ListView_GetSelectedCount(lv);
	UINT newState = MF_BYCOMMAND
		| (selected ? MF_ENABLED : MF_GRAYED);
	EnableMenuItem(params->menu,
		ID_HISTORY_COMMIT_DIFF,
		newState);
	EnableMenuItem(params->menu,
		ID_HISTORY_COMMIT_INFO,
		newState);
}

static void ResizeHistoryDialog(HWND hwnd)
{
	RECT rect;
	HWND lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);
	GetClientRect(hwnd, &rect);
	SetWindowPos(lv, NULL, 0, 0, rect.right, rect.bottom, 0);
}

static BOOL HandleHistoryContextMenu(HWND hwnd, LGitHistoryDialogParams *params, int x, int y)
{
	/* are we in non-client area? */
	RECT clientArea;
	POINT pos = { x, y };
	GetClientRect(hwnd, &clientArea);
	ScreenToClient(hwnd, &pos);
	if (!PtInRect(&clientArea, pos)) {
		return FALSE;
	}
	/* this should be the "commit" menu */
	HMENU commitMenu = GetSubMenu(params->menu, 1);
	TrackPopupMenu(commitMenu, TPM_LEFTALIGN, x, y, 0, hwnd, NULL);
	/* do we need to free GetSubMenu items? */
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
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		LGitSetWindowIcon(hwnd, param->ctx->dllInst, MAKEINTRESOURCE(IDI_HISTORY));
		InitializeHistoryWindow(hwnd, param);
		InitializeHistoryListView(hwnd);
		if (!FillHistoryListView(hwnd, param, param->path_count == 0)) {
			EndDialog(hwnd, 0);
		}
		ResizeHistoryDialog(hwnd);
		UpdateHistoryMenu(hwnd, param);
		return TRUE;
	case WM_SIZE:
		ResizeHistoryDialog(hwnd);
		return TRUE;
	case WM_CONTEXTMENU:
		param = (LGitHistoryDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		return HandleHistoryContextMenu(hwnd, param, LOWORD(lParam), HIWORD(lParam));
	case WM_COMMAND:
		param = (LGitHistoryDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case ID_HISTORY_COMMIT_DIFF:
			ShowSelectedCommitDiff(hwnd, param);
			return TRUE;
		case ID_HISTORY_COMMIT_INFO:
			ShowSelectedCommitInfo(hwnd, param);
			return TRUE;
		case ID_HISTORY_CLOSE:
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		}
		return FALSE;
	case WM_NOTIFY:
		param = (LGitHistoryDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (wParam) {
		case IDC_COMMITHISTORY:
			LPNMHDR child_msg = (LPNMHDR)lParam;
			switch (child_msg->code) {
			case LVN_ITEMACTIVATE:
				/* XXX: Could use fields of NMITEMACTIVATE? */
				ShowSelectedCommitInfo(hwnd, param);
				return TRUE;
			case LVN_ITEMCHANGED:
				UpdateHistoryMenu(hwnd, param);
				return TRUE;
			}
		}
		return FALSE;
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
	SCCRTN ret = SCC_OK;
	git_diff_options diffopts;
	git_pathspec *ps = NULL;
	git_revwalk *walker = NULL;

	LGitHistoryDialogParams params;

	int i, path_count;
	const char *raw_path;
	char **paths = NULL;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccHistory** Context=%p\n", context);
	LGitLog("  files %d\n", nFiles);
	LGitLog("  flags %x\n", dwFlags);

	git_diff_options_init(&diffopts, GIT_DIFF_OPTIONS_VERSION);

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
	}

	diffopts.pathspec.strings = paths;
	diffopts.pathspec.count	  = path_count;
	if (path_count > 0) {
		if (git_pathspec_new(&ps, &diffopts.pathspec) != 0) {
			LGitLibraryError(hWnd, "SccHistory git_pathspec_new");
			ret = SCC_E_NONSPECIFICERROR;
			goto fin;
		}
	}

	if (git_revwalk_new(&walker, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "SccHistory git_revwalk_new");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	if (git_revwalk_push_head(walker) != 0) {
		LGitLibraryError(hWnd, "SccHistory git_revwalk_push_head");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	git_revwalk_sorting(walker, GIT_SORT_TIME);

	params.ctx = ctx;
	params.walker = walker;
	params.ps = ps;
	params.diffopts = &diffopts;
	params.paths = paths;
	params.path_count = path_count;
	params.menu = LoadMenu(ctx->dllInst, MAKEINTRESOURCE(IDR_HISTORY_MENU));
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_COMMITHISTORY),
		hWnd,
		HistoryDialogProc,
		(LPARAM)&params)) {
	case 0:
		LGitLog(" ! Uh-oh, dialog error\n");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	default:
		break;
	}
fin:
	DestroyMenu(params.menu);
	if (ps != NULL) {
		git_pathspec_free(ps);
	}
	if (walker != NULL) {
		git_revwalk_free(walker);
	}
	if (paths != NULL) {
		LGitFreePathList(paths, path_count);
	}
	return ret;
}