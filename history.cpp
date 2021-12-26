/*
 * History of files.
 */

#include "stdafx.h"

static LVCOLUMNW author_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 175, L"Author"
};
static LVCOLUMNW authored_when_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 125, L"Authored At"
};
static LVCOLUMNW comment_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 200, L"Comment"
};
static LVCOLUMNW oid_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 75, L"Object ID"
};

/* 
 * All these variables are a pain to carry indiviually, so we pack a pointer
 * to this in LPARAM. The DialogBoxParam call is anchored to the source of
 * all of this, so they won't expire lifetime-wise past the dialog.
 */
typedef struct _LGitHistoryDialogParams {
	LGitContext *ctx;
	const char *ref; /* NULL is head */
	git_revwalk *walker;
	git_pathspec *ps;
	git_diff_options *diffopts;
	
	char **paths;
	int path_count;

	int max_index;

	BOOL changed;
	BOOL is_head;

	/* window sundry */
	HMENU menu;
} LGitHistoryDialogParams;

static void InitializeHistoryWindow(HWND hwnd, LGitHistoryDialogParams *params)
{
	SetMenu(hwnd, params->menu);
	wchar_t title[256], thing[256];
	/* XXX: Load string resources */
	if (params->path_count == 0 && params->ref == NULL) {
		wcslcpy(title, L"Commit History for Repository", 256);
	} else if (params->path_count == 0) {
		LGitUtf8ToWide(params->ref, thing, 256);
		_snwprintf(title, 256, L"Commit History for %s", thing);
	} else if (params->path_count == 1) {
		LGitUtf8ToWide(params->paths[0], thing, 256);
		_snwprintf(title, 256, L"Commit History for %s", thing);
	} else {
		_snwprintf(title, 256, L"Commit History for %d files", params->path_count);
	}
	SetWindowTextW(hwnd, title);
}

static void InitializeHistoryListView(HWND hwnd, LGitHistoryDialogParams *param)
{
	HWND lv;

	lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);
	if (lv == NULL) {
		return;
	}
	/* Unicode even works on 9x with IE5! */
	ListView_SetUnicodeFormat(lv, TRUE);
	SendMessage(lv, WM_SETFONT, (WPARAM)param->ctx->listviewFont, TRUE);

	ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT
		| LVS_EX_HEADERDRAGDROP
		| LVS_EX_LABELTIP);

	/* There is no A/W version of ListView_InsertColumn, it's always TCHAR */
	SendMessage(lv, LVM_INSERTCOLUMNW, 0, (LPARAM)&oid_column);
	SendMessage(lv, LVM_INSERTCOLUMNW, 1, (LPARAM)&author_column);
	SendMessage(lv, LVM_INSERTCOLUMNW, 2, (LPARAM)&authored_when_column);
	SendMessage(lv, LVM_INSERTCOLUMNW, 3, (LPARAM)&comment_column);
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
	const char *ref = param->ref;

	lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);
	/* clear if we're replenishing */
	ListView_DeleteAllItems(lv);

	/* Push HEAD again */
	if (ref == NULL && git_revwalk_push_head(walker) != 0) {
		LGitLibraryError(hwnd, "History (Pushing Reference)");
		return FALSE;
	}
	if (ref != NULL && git_revwalk_push_ref(walker, ref) != 0) {
		LGitLibraryError(hwnd, "History (Pushing Reference)");
		return FALSE;
	}

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
		wchar_t formatted[256];
		LVITEMW lvi;

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

		if (!whole_repo) {
			int unmatched = parents;

			if (parents == 0) {
				git_tree *tree;
				if (git_commit_tree(&tree, commit) != 0) {
					LGitLibraryError(hwnd, "SccHistory git_commit_tree");
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

		UINT encoding = LGitGitToWindowsCodepage(git_commit_message_encoding(commit));

		/* Actually insert */
		oid_str = git_oid_tostr_s(&oid);
		author = git_commit_author(commit);
		committer = git_commit_committer(commit);
		message = git_commit_message(commit);

		/* Let's inform the dialog. May spam TextOut tho */
		LGitProgressText(param->ctx, oid_str, 1);

		ZeroMemory(&lvi, sizeof(LVITEM));
		lvi.mask = LVIF_TEXT;
		LGitUtf8ToWide(oid_str, formatted, 256);
		lvi.pszText = formatted;
		lvi.iItem = index++;
		lvi.iSubItem = 0;

		lvi.iItem = SendMessage(lv, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
		if (lvi.iItem == -1) {
			LGitLog(" ! ListView_InsertItem failed for %s\n", oid_str);
			continue;
		}
		/* now for the subitems... */
		lvi.iSubItem = 1;
		LGitFormatSignatureW(author, formatted, 256);
		SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);
		
		lvi.iSubItem = 2;
		LGitTimeToStringW(&author->when, formatted, 256);
		SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);
		lvi.iSubItem = 3;
		MultiByteToWideChar(encoding, 0, git_commit_summary(commit), -1, formatted, 256);
		SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);
	}
	LGitProgressDeinit(param->ctx);
	param->max_index = index;
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
	/*
	 * Slightly gross: The normal diff operations shouldn't have the progress
	 * callbacks, because they'd pop up a lot and disturb internal history
	 * build operations. Instead, make a copy with the same settings, just
	 * initialized with the progress callback. This might be pointless since
	 * such a diff could be fast, or maybe it'd be worth having...
	 */
	git_diff_options temp_diffopts;
	memcpy(&temp_diffopts, params->diffopts, sizeof(git_diff_options));
	LGitInitDiffProgressCallback(params->ctx, &temp_diffopts);
	LGitCommitToParentDiff(params->ctx, hwnd, commit, &temp_diffopts);
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
	char oid_s[80];
	ListView_GetItemText(lv, selected, 0, oid_s, 80);
	git_oid oid;
	if (git_oid_fromstr(&oid, oid_s) != 0) {
		LGitLibraryError(hwnd, "git_oid_fromstr");
		return;
	}
	git_commit *commit = NULL;
	if (git_commit_lookup(&commit, params->ctx->repo, &oid) != 0) {
		LGitLibraryError(hwnd, "git_commit_lookup");
		return;
	}
	LGitViewCommitInfo(params->ctx, hwnd, commit, NULL);
}

static void CheckoutSelectedCommit(HWND hwnd, LGitHistoryDialogParams *params)
{
	if (MessageBox(hwnd,
		"Are you sure you want to check out this commit? It will detach the HEAD.",
		"Checkout Commit?",
		MB_ICONQUESTION | MB_YESNO) == IDNO) {
		return;
	}
	HWND lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);
	if (lv == NULL) {
		return;
	}
	int selected = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
	if (selected == -1) {
		return;
	}
	char oid_s[80];
	ListView_GetItemText(lv, selected, 0, oid_s, 80);
	git_oid oid;
	if (git_oid_fromstr(&oid, oid_s) != 0) {
		LGitLibraryError(hwnd, "git_oid_fromstr");
		return;
	}
	if (LGitCheckoutTree(params->ctx, hwnd, &oid) == SCC_OK) {
		FillHistoryListView(hwnd, params, params->path_count == 0);
	}
	params->changed = TRUE;
}

static void RevertSelectedCommit(HWND hwnd, LGitHistoryDialogParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);
	if (lv == NULL) {
		return;
	}
	int selected = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
	if (selected == -1) {
		return;
	}
	char oid_s[80];
	ListView_GetItemText(lv, selected, 0, oid_s, 80);
	git_oid oid;
	if (git_oid_fromstr(&oid, oid_s) != 0) {
		LGitLibraryError(hwnd, "git_oid_fromstr");
		return;
	}
	if (LGitRevertCommit(params->ctx, hwnd, &oid) == SCC_OK) {
		/* History may be mutated */
		FillHistoryListView(hwnd, params, params->path_count == 0);
	}
	params->changed = TRUE;
}

/* This prob shouldn't be shown for the current branch... */
static void CherryPickSelectedCommit(HWND hwnd, LGitHistoryDialogParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);
	if (lv == NULL) {
		return;
	}
	int selected = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
	if (selected == -1) {
		return;
	}
	char oid_s[80];
	ListView_GetItemText(lv, selected, 0, oid_s, 80);
	git_oid oid;
	if (git_oid_fromstr(&oid, oid_s) != 0) {
		LGitLibraryError(hwnd, "git_oid_fromstr");
		return;
	}
	if (LGitCherryPickCommit(params->ctx, hwnd, &oid) == SCC_OK) {
		/* History may be mutated */
		FillHistoryListView(hwnd, params, params->path_count == 0);
	}
	params->changed = TRUE;
}

static void ResetSelectedCommit(HWND hwnd, LGitHistoryDialogParams *params, BOOL hard)
{
	HWND lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);
	if (lv == NULL) {
		return;
	}
	int selected = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
	if (selected == -1) {
		return;
	}
	char oid_s[80];
	ListView_GetItemText(lv, selected, 0, oid_s, 80);
	git_oid oid;
	if (git_oid_fromstr(&oid, oid_s) != 0) {
		LGitLibraryError(hwnd, "git_oid_fromstr");
		return;
	}
	if (LGitResetToCommit(params->ctx, hwnd, &oid, hard) == SCC_OK) {
		/* History may be mutated */
		FillHistoryListView(hwnd, params, params->path_count == 0);
	}
	params->changed = TRUE;
}

static void UpdateHistoryMenu(HWND hwnd, LGitHistoryDialogParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_COMMITHISTORY);
	UINT selected = ListView_GetSelectedCount(lv);
	UINT newState = MF_BYCOMMAND
		| (selected ? MF_ENABLED : MF_GRAYED);
#define EnableMenuItemIfCommitSelected(id) EnableMenuItem(params->menu,id,newState)
	EnableMenuItemIfCommitSelected(ID_HISTORY_COMMIT_DIFF);
	EnableMenuItemIfCommitSelected(ID_HISTORY_COMMIT_INFO);
	EnableMenuItemIfCommitSelected(ID_HISTORY_COMMIT_CHECKOUT);
	EnableMenuItemIfCommitSelected(ID_HISTORY_COMMIT_REVERT);
	EnableMenuItemIfCommitSelected(ID_HISTORY_COMMIT_RESET_HARD);
	EnableMenuItemIfCommitSelected(ID_HISTORY_COMMIT_CHERRYPICK);
	if (params->is_head) {
		/* it doesn't make sense to show for the branch you're on */
		EnableMenuItem(params->menu, ID_HISTORY_COMMIT_CHERRYPICK, MF_BYCOMMAND | MF_GRAYED);
	}
}

static BOOL CALLBACK HistoryDialogProc(HWND hwnd,
									   unsigned int iMsg,
									   WPARAM wParam,
									   LPARAM lParam)
{
	LGitHistoryDialogParams *param;
	param = (LGitHistoryDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitHistoryDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		LGitSetWindowIcon(hwnd, param->ctx->dllInst, MAKEINTRESOURCE(IDI_HISTORY));
		InitializeHistoryWindow(hwnd, param);
		InitializeHistoryListView(hwnd, param);
		if (!FillHistoryListView(hwnd, param, param->path_count == 0)) {
			EndDialog(hwnd, 0);
		}
		LGitControlFillsParentDialog(hwnd, IDC_COMMITHISTORY);
		UpdateHistoryMenu(hwnd, param);
		return TRUE;
	case WM_SIZE:
		LGitControlFillsParentDialog(hwnd, IDC_COMMITHISTORY);
		return TRUE;
	case WM_CONTEXTMENU:
		return LGitContextMenuFromSubmenu(hwnd, param->menu, 1, LOWORD(lParam), HIWORD(lParam));
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_HISTORY_COMMIT_DIFF:
			ShowSelectedCommitDiff(hwnd, param);
			return TRUE;
		case ID_HISTORY_COMMIT_INFO:
			ShowSelectedCommitInfo(hwnd, param);
			return TRUE;
		case ID_HISTORY_COMMIT_CHECKOUT:
			CheckoutSelectedCommit(hwnd, param);
			return TRUE;
		case ID_HISTORY_COMMIT_REVERT:
			RevertSelectedCommit(hwnd, param);
			return TRUE;
		case ID_HISTORY_COMMIT_RESET_HARD:
			ResetSelectedCommit(hwnd, param, TRUE);
			return TRUE;
		case ID_HISTORY_COMMIT_CHERRYPICK:
			CherryPickSelectedCommit(hwnd, param);
			return TRUE;
		case ID_HISTORY_REFRESH:
			FillHistoryListView(hwnd, param, param->path_count == 0);
			return TRUE;
		case ID_HISTORY_CLOSE:
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		}
		return FALSE;
	case WM_NOTIFY:
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

static SCCRTN LGitHistoryInternal(LGitContext *ctx,
								  HWND hWnd, 
								  git_strarray *paths,
								  LONG dwFlags,
								  LPCMDOPTS pvOptions,
								  const char *ref)
{

	SCCRTN ret = SCC_OK;
	git_diff_options diffopts;
	git_pathspec *ps = NULL;
	git_revwalk *walker = NULL;

	LGitHistoryDialogParams params;
	ZeroMemory(&params, sizeof(LGitHistoryDialogParams));

	git_diff_options_init(&diffopts, GIT_DIFF_OPTIONS_VERSION);

	if (paths != NULL) {
		diffopts.pathspec.strings = paths->strings;
		diffopts.pathspec.count	  = paths->count;
		if (paths->count > 0) {
			if (git_pathspec_new(&ps, &diffopts.pathspec) != 0) {
				LGitLibraryError(hWnd, "SccHistory git_pathspec_new");
				ret = SCC_E_NONSPECIFICERROR;
				goto fin;
			}
		}
		params.paths = paths->strings;
		params.path_count = paths->count;
	}

	if (git_revwalk_new(&walker, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "SccHistory git_revwalk_new");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	git_revwalk_sorting(walker, GIT_SORT_TIME);

	/* are we using the HEAD? */
	if (ref != NULL && strlen(ref) > 0) {
		/* it's a branch because only those can be a HEAD */
		git_reference *possible_head = NULL;
		/* failures here are insignificant */
		if (git_reference_lookup(&possible_head, ctx->repo, ref) != 0) {
			params.is_head = FALSE;
		} else {
			params.is_head = git_branch_is_head(possible_head);
			git_reference_free(possible_head);
		}
	} else {
		/* default is to use HEAD, so yes */
		params.is_head = TRUE;
	}

	params.ctx = ctx;
	params.walker = walker;
	params.ps = ps;
	params.diffopts = &diffopts;
	params.ref = ref;
	params.menu = LoadMenu(ctx->dllInst, MAKEINTRESOURCE(IDR_HISTORY_MENU));
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_COMMITHISTORY),
		hWnd,
		HistoryDialogProc,
		(LPARAM)&params)) {
	case 0:
	case -1:
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
	if (params.changed) {
		ret = SCC_I_RELOADFILE;
	}
	return ret;
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
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccHistory** Context=%p\n", context);
	LGitLog("  files %d\n", nFiles);
	LGitLog("  flags %x\n", dwFlags);

	git_strarray strings;
	ZeroMemory(&strings, sizeof(git_strarray));
	if (nFiles > 0) {
		char **paths = NULL;
		int i, path_count = 0;
		paths = (char**)calloc(sizeof(char*), nFiles);
		if (paths == NULL) {
			return SCC_E_NONSPECIFICERROR;
		}
		for (i = 0; i < nFiles; i++) {
			char *path = LGitAnsiToUtf8Alloc(lpFileNames[i]);
			const char *raw_path = LGitStripBasePath(ctx, path);
			if (raw_path == NULL) {
				LGitLog("    Couldn't get base path for %s\n", path);
				free(path);
				continue;
			}
			/* Translate because libgit2 operates with forward slashes */
			LGitTranslateStringChars(path, '\\', '/');
			LGitLog("    %s\n", raw_path);
			paths[path_count++] = (char*)raw_path;
		}
		strings.strings = paths;
		strings.count = path_count;
	}
	SCCRTN ret = LGitHistoryInternal(ctx, hWnd, &strings, dwFlags, pvOptions, NULL);
	if (strings.strings != NULL) {
		LGitFreePathList(strings.strings, strings.count);
	}
	return ret;
}

SCCRTN LGitHistoryForRefByName(LPVOID context, 
							   HWND hWnd, 
							   const char *ref)
{
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**LGitHistoryForRefByName** Context=%p\n", context);
	LGitLog("  ref %s\n", ref);
	return LGitHistoryInternal(ctx, hWnd, NULL, 0, NULL, ref);
}

SCCRTN LGitHistory(LPVOID context, 
				   HWND hWnd, 
				   git_strarray *paths)
{
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**LGitHistory** Context=%p\n", context);
	if (paths != NULL) {
		LGitLog("  path count %d\n", paths->count);
	}
	return LGitHistoryInternal(ctx, hWnd, paths, 0, NULL, NULL);
}