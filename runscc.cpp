/*
 * The SccRunScc/"Explorer" UI that does everything the IDE can't.
 */

#include "stdafx.h"

#define STATUS_BAR_PART_COUNT 3

typedef struct _LGitExplorerParams {
	LGitContext *ctx;
	HMENU menu;
	HWND status_bar;
	int status_bar_parts[STATUS_BAR_PART_COUNT];
	std::set<std::string> *initial_select;
	BOOL standalone;
	/* for the view */
	BOOL include_ignored, include_unmodified, include_untracked;
} LGitExplorerParams;

/* put here for convenience */
static LVCOLUMN name_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 400, "Name"
};
static LVCOLUMN status_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 125, "Status"
};

static void InitStandaloneExplorer(HWND hwnd, LGitExplorerParams *params)
{
	/*
	 *These commands shouldn't be available because they don't make sense in
	 * the context of an IDE.
	 */
	if (!params->standalone) {
		DeleteMenu(params->menu, ID_EXPLORER_REPOSITORY_OPEN, MF_BYCOMMAND);
		return;
	}
	LONG style = GetWindowLong(hwnd, GWL_STYLE);
	style |= WS_MINIMIZEBOX;
	SetWindowLong(hwnd, GWL_STYLE, style);
}

static WNDPROC ListViewProc;

static LRESULT CALLBACK ExplorerListViewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_DROPFILES:
		SendMessage(GetParent(hwnd), msg, wParam, lParam);
		break;
	}
	return CallWindowProc(ListViewProc, hwnd, msg, wParam, lParam);
}

static void InitExplorerListView(HWND hwnd, LGitExplorerParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_EXPLORER_FILES);

	/* HACK: It won't process WM_DROPFILES without it */
	ListViewProc = (WNDPROC)GetWindowLong(lv, GWL_WNDPROC);
	SetWindowLong(lv, GWL_WNDPROC, (long)ExplorerListViewProc);

	ListView_InsertColumn(lv, 0, &name_column);
	ListView_InsertColumn(lv, 1, &status_column);

	/* Initialize the system image list */
	HIMAGELIST sil = LGitGetSystemImageList();
	ListView_SetImageList(lv, sil, LVSIL_SMALL);
}

typedef struct _LGitExplorerStatusCallbackParams {
	LGitContext *ctx;
	HWND hwnd, lv;
	int index;
	std::set<std::string> *initial_select;
} LGitExplorerStatusCallbackParams;

static BOOL StatusToString(unsigned int flags, wchar_t *buf, size_t bufsz)
{
	if (bufsz < 1) {
		return FALSE;
	}
	int index = 0;
	buf[0] = L'\0';
	/* now for the flags */
#define AppendIfStatus(flag,str) if (flags & flag) { if (index++) wcslcat(buf, L", ", bufsz); wcslcat(buf, str, bufsz); }
	AppendIfStatus(GIT_STATUS_INDEX_NEW, L"New in stage");
	AppendIfStatus(GIT_STATUS_INDEX_MODIFIED, L"Changed in stage");
	AppendIfStatus(GIT_STATUS_INDEX_DELETED, L"Deleted from stage");
	AppendIfStatus(GIT_STATUS_INDEX_TYPECHANGE, L"Type changed in stage");
	AppendIfStatus(GIT_STATUS_WT_NEW, L"New");
	AppendIfStatus(GIT_STATUS_WT_MODIFIED, L"Changed");
	AppendIfStatus(GIT_STATUS_WT_DELETED, L"Deleted");
	AppendIfStatus(GIT_STATUS_WT_TYPECHANGE, L"Type changed");
	AppendIfStatus(GIT_STATUS_WT_RENAMED, L"Renaming");
	AppendIfStatus(GIT_STATUS_WT_UNREADABLE, L"Unreadable");
	AppendIfStatus(GIT_STATUS_IGNORED, L"Ignored");
	AppendIfStatus(GIT_STATUS_CONFLICTED, L"Conflicting");
	return TRUE;
}

static int FillStatusItem(const char *relative_path,
						  unsigned int flags,
						  void *context)
{
	LVITEMW lvi;

	LGitExplorerStatusCallbackParams *params = (LGitExplorerStatusCallbackParams*)context;

	/* Get the system image list index for the file, needing the full path */
	SHFILEINFOW sfi;
	ZeroMemory(&sfi, sizeof(sfi));
	wchar_t path[2048], relative_path_utf16[2048];
	LGitUtf8ToWide(params->ctx->workdir_path, path, 2048);
	LGitUtf8ToWide(relative_path, relative_path_utf16, 2048);
	wcslcat(path, relative_path_utf16, 2048);
	LGitTranslateStringCharsW(path, L'/', L'\\');
	/* XXX: Should this be cached i.e. by extension? */
	SHGetFileInfoW(path, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);

	/*
	 * Unlike the equivalent in query.cpp, we can just turn a libgit2 flags
	 * field into a string.
	 */
	ZeroMemory(&lvi, sizeof(LVITEMW));
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
	lvi.pszText = (wchar_t*)relative_path_utf16;
	lvi.iImage = sfi.iIcon;
	if (params->initial_select->count(relative_path)) {
		lvi.state = LVIS_SELECTED;
	}
	lvi.iItem = params->index;
	lvi.iSubItem = 0;
	lvi.iItem = SendMessage(params->lv, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
	if (lvi.iItem == -1) {
		LGitLog(" ! ListView_InsertItem failed\n");
		return GIT_EUSER;
	}
	
	wchar_t msg[256];
	StatusToString(flags, msg, 256);
	lvi.mask = LVIF_TEXT;
	lvi.pszText = (wchar_t*)msg;
	lvi.iItem = params->index;
	lvi.iSubItem = 1;
	SendMessage(params->lv, LVM_SETITEMW, 0, (LPARAM)&lvi);

	params->index++;
	return 0;
}

static void FillExplorerListView(HWND hwnd, LGitExplorerParams *params)
{
	git_status_options sopts;
	LGitExplorerStatusCallbackParams cbp;
	git_status_options_init(&sopts, GIT_STATUS_OPTIONS_VERSION);
	HWND lv = GetDlgItem(hwnd, IDC_EXPLORER_FILES);

	/* See the notes for the git_status_options use in query,cpp... */
	sopts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	/* XXX: Make configurable */
	sopts.flags = GIT_STATUS_OPT_INCLUDE_UNREADABLE;
	if (params->include_untracked) {
		sopts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED
			| GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;
	}
	if (params->include_unmodified) {
		sopts.flags |= GIT_STATUS_OPT_INCLUDE_UNMODIFIED;
	}
	if (params->include_ignored) {
		sopts.flags |= GIT_STATUS_OPT_INCLUDE_IGNORED
			| GIT_STATUS_OPT_RECURSE_IGNORED_DIRS;
	}

	cbp.ctx = params->ctx;
	cbp.hwnd = hwnd;
	cbp.lv = lv;
	cbp.index = 0;
	cbp.initial_select = params->initial_select;

	ListView_DeleteAllItems(lv);
	if (params->ctx->repo == NULL) {
		return;
	}
	/* This gets us a stage scan too */
	git_status_foreach_ext(params->ctx->repo, &sopts, FillStatusItem, &cbp);
}

static void GetHeadState(HWND hwnd, LGitExplorerParams *params, char *buf, size_t bufsz)
{
	git_reference *head = NULL;
	const char *ref = NULL;
	switch (git_repository_head(&head, params->ctx->repo)) {
	case GIT_EUNBORNBRANCH:
		strlcpy(buf, "Unborn HEAD", bufsz);
		return;
	case GIT_ENOTFOUND:
		strlcpy(buf, "No HEAD", bufsz);
		return;
	default:
		LGitLibraryError(hwnd, "git_repository_head");
		strlcpy(buf, "Error", bufsz);
		return;
	case 0:
		/* A detached HEAD will result in a resolved HEAD with name of HEAD */
		if (git_repository_head_detached(params->ctx->repo)) {
			/* use commit ID */
			git_oid commit_oid;
			if (git_reference_name_to_id(&commit_oid, params->ctx->repo, "HEAD") != 0) {
				LGitLibraryError(hwnd, "git_reference_name_to_id");
				strlcpy(buf, "Error getting HEAD commit", bufsz);
				goto err;
			}
			strlcpy(buf, "Detached, ", bufsz);
			strlcat(buf, git_oid_tostr_s(&commit_oid), bufsz);
		} else {
			ref = git_reference_shorthand(head);
			strlcpy(buf, ref == NULL ? "HEAD without branch" : ref, bufsz);
		}
		break;
	}
err:
	if (head != NULL) {
		git_reference_free(head);
	}
}

static BOOL GetVisibleItemsText(HWND hwnd, LGitExplorerParams *params, char *buf, size_t bufsz)
{
	if (bufsz < 1) {
		return FALSE;
	}
	int index = 0;
	strlcpy(buf, "Showing ", bufsz);
#define AppendIfVisible(flag,str) if (flag) { if (index++) strlcat(buf, ", ", bufsz); strlcat(buf, str, bufsz); }
	AppendIfVisible(params->include_ignored, "Ignored");
	AppendIfVisible(params->include_unmodified, "Unchanged");
	AppendIfVisible(params->include_untracked, "Untracked");
	if (index == 0) {
		strlcat(buf, "Only Changed", bufsz);
	}
	return TRUE;
}

static void UpdateExplorerStatus(HWND hwnd, LGitExplorerParams *params)
{
	char newTitle[512], statusText[128], visibleText[128];
	const char *stateText = "";
	params->status_bar_parts[2] = -1;
	if (params->ctx->active && params->ctx->repo != NULL) {
		int state = git_repository_state(params->ctx->repo);
		stateText = LGitRepoStateString(state);
		GetHeadState(hwnd, params, statusText, 128);
		strlcpy(newTitle, params->ctx->workdir_path, 512);
		/* add some padding, plus account for border on sizes */
		params->status_bar_parts[0] = LGitMeasureWidth(params->status_bar, stateText) +
			(GetSystemMetrics(SM_CXBORDER) * 4) + 10;
		GetVisibleItemsText(hwnd, params, visibleText, 128);
		params->status_bar_parts[1] = params->status_bar_parts[0] +
			LGitMeasureWidth(params->status_bar, visibleText) +
			(GetSystemMetrics(SM_CXBORDER) * 4) + 10;
		LGitLog(" ! %d %d\n", params->status_bar_parts[0], params->status_bar_parts[1]);
	} else {
		strlcpy(newTitle, "(no repo)", 512);
		strlcpy(statusText, "", 128);
		params->status_bar_parts[0] = 0;
		params->status_bar_parts[1] = 0;
	}
	SetWindowText(hwnd, newTitle);
	SendMessage(params->status_bar, SB_SETPARTS, STATUS_BAR_PART_COUNT, (LPARAM)params->status_bar_parts);
	SendMessage(params->status_bar, SB_SETTEXT, 0, (LPARAM)stateText);
	SendMessage(params->status_bar, SB_SETTEXT, 1, (LPARAM)visibleText);
	SendMessage(params->status_bar, SB_SETTEXT, 2, (LPARAM)statusText);
}

/* doesn't handle more than one */
static BOOL GetSingleSelection(HWND hwnd, char *buf, size_t bufsz)
{
	HWND lv = GetDlgItem(hwnd, IDC_EXPLORER_FILES);
	if (lv == NULL) {
		return FALSE;
	}
	int selected = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
	if (selected == -1) {
		return FALSE;
	}
	/* Convert to UTF-16, but then to UTF-8 immediately bc libgit2 mmostly */
	wchar_t temp_buf[1024];
	LVITEMW lvi;
	ZeroMemory(&lvi, sizeof(lvi));
	lvi.mask = LVIF_TEXT;
	lvi.iItem = selected; /* in case */
	lvi.pszText = temp_buf;
	lvi.cchTextMax = 1024;
	if (SendMessage(lv, LVM_GETITEMTEXTW, selected, (LPARAM)&lvi) < 1) {
		return FALSE;
	}
	LGitWideToUtf8(temp_buf, buf, bufsz);
	return TRUE;
}

/*
 * caller must free selection->strings and each member (LGitFreePathList),
 * technically git_strarray_dispose can be used but it calls its own free.
 * we're using the same libc, but still sus
 */
static BOOL GetMultipleSelection(HWND hwnd, git_strarray *selection)
{
	HWND lv = GetDlgItem(hwnd, IDC_EXPLORER_FILES);
	if (lv == NULL) {
		return FALSE;
	}
	UINT count = ListView_GetSelectedCount(lv);
	char **strings = (char**)calloc(count, sizeof(char*));
	if (strings == NULL) {
		return FALSE;
	}
	selection->count = count;
	selection->strings = strings;

	int index = 0, selected = -1;
	wchar_t temp_buf[1024];
	while ((selected = ListView_GetNextItem(lv, selected, LVNI_SELECTED)) != -1) {
		/* XXX: measure somehow */
		char *buf = (char*)malloc(1024);
		LVITEMW lvi;
		ZeroMemory(&lvi, sizeof(lvi));
		lvi.mask = LVIF_TEXT;
		lvi.iItem = selected; /* in case */
		lvi.pszText = temp_buf;
		lvi.cchTextMax = 1024;
		if (SendMessage(lv, LVM_GETITEMTEXTW, selected, (LPARAM)&lvi) < 1) {
			return FALSE;
		}
		LGitWideToUtf8(temp_buf, buf, 1024);
		strings[index++] = buf;
	}
	return TRUE;
}

static void SelectedFileProperties(HWND hwnd, LGitExplorerParams *params)
{
	/* the function only takes a single file ;) */
	char path[1024];
	if (!GetSingleSelection(hwnd, path, 1024)) {
		return;
	}
	LGitFileProperties(params->ctx, hwnd, path);
}

static void UpdateExplorerMenu(HWND hwnd, LGitExplorerParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_EXPLORER_FILES);
	UINT selected = ListView_GetSelectedCount(lv);
	BOOL active = params->ctx->active;
	int unborn = active ? git_repository_head_unborn(params->ctx->repo) : -1;
	/* Because we can be invoked without a valid repo... */
	UINT newState = MF_IF_CMD(active);
	UINT newStateBorn = MF_IF_CMD(active && unborn == 0);
	UINT newStateSelected = MF_IF_CMD(active && selected > 0);
	UINT newStateSelectedBorn = MF_IF_CMD(active && selected > 0 && unborn == 0);
	UINT newStateSelectedSingle = MF_IF_CMD(active && selected == 1);
#define EnableMenuItemIfInRepo(id) EnableMenuItem(params->menu,id,newState)
#define EnableMenuItemIfInRepoAndBorn(id) EnableMenuItem(params->menu,id,newStateBorn)
#define EnableMenuItemIfSelected(id) EnableMenuItem(params->menu,id,newStateSelected)
#define EnableMenuItemIfSelectedAndBorn(id) EnableMenuItem(params->menu,id,newStateSelectedBorn)
#define EnableMenuItemIfSelectedSingle(id) EnableMenuItem(params->menu,id,newStateSelectedSingle)
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_OPENINWINDOWS);
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_REFRESH);
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_DIFFFROMSTAGE);
	EnableMenuItemIfInRepo(ID_EXPLORER_DIFF_DIFFFROMREVISION);
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_APPLYPATCH);
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_BRANCHES);
	EnableMenuItemIfInRepoAndBorn(ID_EXPLORER_REPOSITORY_HISTORY);
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_CHECKOUT);
	EnableMenuItemIfInRepo(ID_EXPLORER_STAGE_ADDFILES);
	EnableMenuItemIfInRepo(ID_EXPLORER_STAGE_ADDALL);
	EnableMenuItemIfInRepo(ID_EXPLORER_STAGE_UPDATEALL);
	EnableMenuItemIfSelected(ID_EXPLORER_STAGE_UPDATE);
	EnableMenuItemIfSelected(ID_EXPLORER_STAGE_REMOVE);
	EnableMenuItemIfSelected(ID_EXPLORER_STAGE_UNSTAGE);
	EnableMenuItemIfSelected(ID_EXPLORER_STAGE_REVERTTOSTAGED);
	EnableMenuItemIfSelectedAndBorn(ID_EXPLORER_STAGE_REVERTTOHEAD);
	EnableMenuItemIfSelectedSingle(ID_EXPLORER_STAGE_FILEPROPERTIES);
	EnableMenuItemIfSelected(ID_EXPLORER_FILE_OPEN);
	EnableMenuItemIfSelectedAndBorn(ID_EXPLORER_FILE_HISTORY);
	EnableMenuItemIfSelected(ID_EXPLORER_FILE_DIFFFROMSTAGE);
	EnableMenuItemIfSelected(ID_EXPLORER_FILE_DIFFFROMREVISION);
	EnableMenuItemIfInRepo(ID_EXPLORER_STAGE_COMMITSTAGED);
	EnableMenuItemIfInRepoAndBorn(ID_EXPLORER_STAGE_AMENDLASTCOMMIT);
	EnableMenuItemIfInRepo(ID_EXPLORER_REMOTE_MANAGEREMOTES);
	EnableMenuItemIfInRepo(ID_EXPLORER_REMOTE_PUSH);
	EnableMenuItemIfInRepo(ID_EXPLORER_REMOTE_PULL);
	EnableMenuItemIfInRepo(ID_EXPLORER_VIEW_SHOWUNTRACKED);
	CheckMenuItemIf(params->menu, ID_EXPLORER_VIEW_SHOWUNTRACKED, params->include_untracked);
	EnableMenuItemIfInRepo(ID_EXPLORER_VIEW_SHOWUNCHANGED);
	CheckMenuItemIf(params->menu, ID_EXPLORER_VIEW_SHOWUNCHANGED, params->include_unmodified);
	EnableMenuItemIfInRepo(ID_EXPLORER_VIEW_SHOWIGNORED);
	CheckMenuItemIf(params->menu, ID_EXPLORER_VIEW_SHOWIGNORED, params->include_ignored);
	EnableMenuItemIfInRepo(ID_EXPLORER_CONFIG_REPOSITORY);
}

/* This only makes sense in the context of a standalone instance. */
static BOOL OpenRepository(HWND hwnd, LGitExplorerParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_EXPLORER_FILES);
	char proj_name[SCC_PRJPATH_SIZE], path[SCC_PRJPATH_LEN], user[SCC_USER_SIZE], aux_path[SCC_AUXLABEL_SIZE];
	BOOL is_new = TRUE, retbool = TRUE;
	SCCRTN ret;

	ZeroMemory(aux_path, SCC_AUXLABEL_SIZE);
	ZeroMemory(proj_name, SCC_PRJPATH_SIZE);
	ZeroMemory(path, SCC_PRJPATH_SIZE);
	strlcpy(user, params->ctx->username, SCC_USER_SIZE); /* who cares */
	
	/* don't bother with GetProjPath */
	ret = LGitClone(params->ctx, hwnd, proj_name, path, &is_new);
	if (ret == SCC_I_OPERATIONCANCELED) {
		return FALSE;
	} else if (ret != SCC_OK) {
		LGitLibraryError(hwnd, "Selecting Different Project");
		return FALSE;
	}
	/* we got it, let's go */
	ret = SccCloseProject(params->ctx); /* always succeeds */
	ret = SccOpenProject(params->ctx, hwnd, user, proj_name, path, aux_path, "", params->ctx->textoutCb, SCC_OP_CREATEIFNEW);
	if (ret != SCC_OK && ret != SCC_I_OPERATIONCANCELED) {
		LGitLibraryError(hwnd, "Opening Different Project");
		retbool = FALSE;
	}
	UpdateExplorerStatus(hwnd, params);
	if (params->ctx->repo == NULL) {
		/* We're gonna do stuff we can only do with i.e. a stage */
		ListView_DeleteAllItems(lv);
		EnableWindow(lv, FALSE);
	} else {
		EnableWindow(lv, TRUE);
		FillExplorerListView(hwnd, params);
	}
	UpdateExplorerMenu(hwnd, params);
	return retbool;
}

static void DiffFromRevision(HWND hwnd, LGitExplorerParams *params, git_strarray *paths)
{
	git_object *obj = NULL;
	git_reference *ref = NULL;
	git_tree *tree = NULL;
	if (LGitRevparseDialog(params->ctx, hwnd, "Diff from Revision", "HEAD", &obj, &ref) == SCC_OK) {
		if (git_object_peel((git_object**)&tree, obj, GIT_OBJECT_TREE) != 0) {
			LGitLibraryError(hwnd, "Couldn't Peel Tree");
		} else {
			LGitDiffTreeToWorkdir(params->ctx, hwnd, paths, tree);
		}
	}
	git_tree_free(tree);
	git_object_free(obj);
	git_reference_free(ref);
}

static BOOL HandleExplorerCommand(HWND hwnd, UINT cmd, LGitExplorerParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_EXPLORER_FILES);
	git_strarray strings;
	ZeroMemory(&strings, sizeof(git_strarray));
	int ret;
	switch (cmd) {
	case ID_EXPLORER_REPOSITORY_OPEN:
		OpenRepository(hwnd, params);
		return TRUE;
	case ID_EXPLORER_REPOSITORY_OPENINWINDOWS:
		{
			SHELLEXECUTEINFOW info;
			ZeroMemory(&info, sizeof(SHELLEXECUTEINFO));
			wchar_t path[2048];
			LGitUtf8ToWide(params->ctx->workdir_path, path, 2048);

			info.cbSize = sizeof info;
			info.lpFile = path;
			info.nShow = SW_SHOW;
			info.fMask = SEE_MASK_INVOKEIDLIST;
			info.lpVerb = L"open";

			ShellExecuteExW(&info);
		}
		return TRUE;
	case ID_EXPLORER_REPOSITORY_REFRESH:
		UpdateExplorerStatus(hwnd, params);
		FillExplorerListView(hwnd, params);
		UpdateExplorerMenu(hwnd, params);
		return TRUE;
	case ID_EXPLORER_REPOSITORY_DIFFFROMSTAGE:
		LGitDiffStageToWorkdir(params->ctx, hwnd, NULL);
		return TRUE;
	case ID_EXPLORER_DIFF_DIFFFROMREVISION:
		DiffFromRevision(hwnd, params, NULL);
		return TRUE;
	case ID_EXPLORER_REPOSITORY_APPLYPATCH:
		if (LGitApplyPatchDialog(params->ctx, hwnd) == SCC_OK) {
			UpdateExplorerStatus(hwnd, params);
			FillExplorerListView(hwnd, params);
			UpdateExplorerMenu(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_REPOSITORY_BRANCHES:
		LGitShowBranchManager(params->ctx, hwnd);
		/* XXX: Only if something changed as a result */
		UpdateExplorerStatus(hwnd, params);
		FillExplorerListView(hwnd, params);
		UpdateExplorerMenu(hwnd, params);
		return TRUE;
	case ID_EXPLORER_REPOSITORY_HISTORY:
		SccHistory(params->ctx, hwnd, 0, NULL, 0, NULL);
		/* XXX: Only if something changed as a result */
		UpdateExplorerStatus(hwnd, params);
		FillExplorerListView(hwnd, params);
		UpdateExplorerMenu(hwnd, params);
		return TRUE;
	case ID_EXPLORER_REPOSITORY_CHECKOUT:
		{
			git_object *obj = NULL;
			git_reference *ref = NULL;
			if (LGitRevparseDialog(params->ctx, hwnd, "Switch to Revision", "", &obj, &ref) == SCC_OK) {
				if (ref != NULL) {
					LGitCheckoutRef(params->ctx, hwnd, ref);
				} else if (obj != NULL) {
					/* automatically peeled */
					LGitCheckoutTree(params->ctx, hwnd, git_object_id(obj));
				}
				UpdateExplorerStatus(hwnd, params);
				FillExplorerListView(hwnd, params);
				UpdateExplorerMenu(hwnd, params);
			}
			git_object_free(obj);
			git_reference_free(ref);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_ADDFILES:
		if (LGitStageAddDialog(params->ctx, hwnd) == SCC_OK) {
			FillExplorerListView(hwnd, params);
			UpdateExplorerMenu(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_UPDATE: /* could also stage WT new, so NOT update */
		if (GetMultipleSelection(hwnd, &strings)) {
			LGitStageAddFiles(params->ctx, hwnd, &strings, FALSE);
			LGitFreePathList(strings.strings, strings.count);
			FillExplorerListView(hwnd, params);
			UpdateExplorerMenu(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_ADDALL:
		LGitStageAddFiles(params->ctx, hwnd, &strings, FALSE);
		FillExplorerListView(hwnd, params);
		UpdateExplorerMenu(hwnd, params);
		return TRUE;
	case ID_EXPLORER_STAGE_UPDATEALL:
		LGitStageAddFiles(params->ctx, hwnd, &strings, TRUE);
		FillExplorerListView(hwnd, params);
		UpdateExplorerMenu(hwnd, params);
		return TRUE;
	case ID_EXPLORER_STAGE_REMOVE:
		ret = MessageBox(hwnd,
			"The selected files will no longer be tracked by Git. "
			"It will not delete the files. Are you sure?",
			"Really Remove?",
			MB_ICONWARNING | MB_YESNO);
		if (ret != IDYES) {
			return TRUE;
		}
		if (GetMultipleSelection(hwnd, &strings)) {
			LGitStageRemoveFiles(params->ctx, hwnd, &strings);
			LGitFreePathList(strings.strings, strings.count);
			FillExplorerListView(hwnd, params);
			UpdateExplorerMenu(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_UNSTAGE:
		/* This is non-destructive */
		if (GetMultipleSelection(hwnd, &strings)) {
			LGitStageUnstageFiles(params->ctx, hwnd, &strings);
			LGitFreePathList(strings.strings, strings.count);
			FillExplorerListView(hwnd, params);
			UpdateExplorerMenu(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_REVERTTOSTAGED:
		ret = MessageBox(hwnd,
			"All changes in the selected files will be lost. Are you sure?",
			"Really Revert?",
			MB_ICONWARNING | MB_YESNO);
		if (ret != IDYES) {
			return TRUE;
		}
		if (GetMultipleSelection(hwnd, &strings)) {
			LGitCheckoutStaged(params->ctx, hwnd, &strings);
			LGitFreePathList(strings.strings, strings.count);
			FillExplorerListView(hwnd, params);
			UpdateExplorerMenu(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_REVERTTOHEAD:
		ret = MessageBox(hwnd,
			"All changes in the selected files will be lost. Are you sure?",
			"Really Revert?",
			MB_ICONWARNING | MB_YESNO);
		if (ret != IDYES) {
			return TRUE;
		}
		if (GetMultipleSelection(hwnd, &strings)) {
			/* XXX: Does unstaging make sense as well here? */
			LGitCheckoutHead(params->ctx, hwnd, &strings);
			LGitFreePathList(strings.strings, strings.count);
			FillExplorerListView(hwnd, params);
			UpdateExplorerMenu(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_FILEPROPERTIES:
		SelectedFileProperties(hwnd, params);
		return TRUE;
	case ID_EXPLORER_FILE_OPEN:
		if (!params->standalone) {	
			ret = MessageBox(hwnd,
				"This will open the files in new instances of the default programs.\r\n\r\n"
				"If you intend to open them in the IDE, you should open them from the IDE instead.\r\n"
				"Are you sure you want to open the other programs?",
				"Really Open?",
				MB_ICONWARNING | MB_YESNO);
			if (ret != IDYES) {
				return TRUE;
			}
		}
		if (GetMultipleSelection(hwnd, &strings)) {
			LGitOpenFiles(params->ctx, &strings);
			LGitFreePathList(strings.strings, strings.count);
		}
		return TRUE;
	case ID_EXPLORER_FILE_HISTORY:
		if (GetMultipleSelection(hwnd, &strings)) {
			LGitHistory(params->ctx, hwnd, &strings);
			LGitFreePathList(strings.strings, strings.count);
		}
		return TRUE;
	case ID_EXPLORER_FILE_DIFFFROMSTAGE:
		if (GetMultipleSelection(hwnd, &strings)) {
			LGitDiffStageToWorkdir(params->ctx, hwnd, &strings);
			LGitFreePathList(strings.strings, strings.count);
		}
		return TRUE;
	case ID_EXPLORER_FILE_DIFFFROMREVISION:
		if (GetMultipleSelection(hwnd, &strings)) {
			DiffFromRevision(hwnd, params, &strings);
			LGitFreePathList(strings.strings, strings.count);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_COMMITSTAGED:
		if (LGitCreateCommitDialog(params->ctx, hwnd, FALSE, NULL, NULL) == SCC_OK) {
			UpdateExplorerMenu(hwnd, params);
			FillExplorerListView(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_AMENDLASTCOMMIT:
		if (LGitCreateCommitDialog(params->ctx, hwnd, TRUE, NULL, NULL) == SCC_OK) {
			UpdateExplorerMenu(hwnd, params);
			FillExplorerListView(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_REMOTE_MANAGEREMOTES:
		LGitShowRemoteManager(params->ctx, hwnd);
		return TRUE;
	case ID_EXPLORER_REMOTE_PUSH:
		LGitPushDialog(params->ctx, hwnd);
		return TRUE;
	case ID_EXPLORER_REMOTE_PULL:
		LGitPullDialog(params->ctx, hwnd);
		return TRUE;
	case ID_EXPLORER_CONFIG_REPOSITORY:
		{
			git_config *config = NULL;
			if (git_repository_config(&config, params->ctx->repo) != 0) {
				LGitLibraryError(hwnd, "git_repository_config");
				return true;
			}
			/* XXX: Do we force a scope with git_config_open_level? */
			LGitManageConfig(params->ctx, hwnd, config, "Repository");
			git_config_free(config);
		}
		return TRUE;
	case ID_EXPLORER_CONFIG_GLOBAL:
		{
			git_config *config = NULL;
			/* Covers the sys-wide to read from, even if we only edit user */
			if (git_config_open_default(&config) != 0) {
				LGitLibraryError(hwnd, "git_config_open_default");
				return true;
			}
			LGitManageConfig(params->ctx, hwnd, config, "Global");
			git_config_free(config);
		}
		return TRUE;
	case ID_EXPLORER_VIEW_SHOWUNTRACKED:
		params->include_untracked = !params->include_untracked;
		UpdateExplorerStatus(hwnd, params);
		FillExplorerListView(hwnd, params);
		UpdateExplorerMenu(hwnd, params);
		return TRUE;
	case ID_EXPLORER_VIEW_SHOWUNCHANGED:
		params->include_unmodified = !params->include_unmodified;
		UpdateExplorerStatus(hwnd, params);
		FillExplorerListView(hwnd, params);
		UpdateExplorerMenu(hwnd, params);
		return TRUE;
	case ID_EXPLORER_VIEW_SHOWIGNORED:
		params->include_ignored = !params->include_ignored;
		UpdateExplorerStatus(hwnd, params);
		FillExplorerListView(hwnd, params);
		UpdateExplorerMenu(hwnd, params);
		return TRUE;
	case ID_EXPLORER_HELP_ABOUT:
		LGitAbout(hwnd, params->ctx);
		return TRUE;
	default:
		return FALSE;
	}
}

static void InitExplorerView(HWND hwnd, LGitExplorerParams *params)
{
	/* XXX: do not hardcode the dialog ID */
	params->status_bar = CreateStatusWindow(WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, "Visual Git", hwnd, 999);
	HWND lv = GetDlgItem(hwnd, IDC_EXPLORER_FILES);
	LGitSetWindowIcon(hwnd, params->ctx->dllInst, MAKEINTRESOURCE(IDI_LGIT));
	SetMenu(hwnd, params->menu);
	UpdateExplorerStatus(hwnd, params);
	UpdateExplorerMenu(hwnd, params);
	if (params->ctx->repo == NULL) {
		/* We're gonna do stuff we can only do with i.e. a stage */
		ListView_DeleteAllItems(lv);
		return;
	}
	/* it's tempting to disable the listview but it gives visual oddities */
}

static void ResizeExplorerView(HWND hwnd, LGitExplorerParams *params)
{
	SendMessage(params->status_bar, WM_SIZE, 0, 0);
	RECT carveout = {0, 0, 0, 0};
	GetClientRect(params->status_bar, &carveout);
	carveout.top = 0;
	carveout.left = 0;
	carveout.right = 0;
	LGitControlFillsParentDialogCarveout(hwnd, IDC_EXPLORER_FILES, &carveout);
}

static BOOL CALLBACK ExplorerDialogProc(HWND hwnd,
										unsigned int iMsg,
										WPARAM wParam,
										LPARAM lParam)
{
	LGitExplorerParams *param;
	param = (LGitExplorerParams*)GetWindowLong(hwnd, GWL_USERDATA);
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitExplorerParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitStandaloneExplorer(hwnd, param);
		InitExplorerListView(hwnd, param);
		InitExplorerView(hwnd, param);
		ResizeExplorerView(hwnd, param);
		FillExplorerListView(hwnd, param);
		/* empty the selection that we no longer need it */
		param->initial_select->clear();
		if (!param->ctx->active && param->standalone) {
			OpenRepository(hwnd, param);
		}
		return TRUE;
	case WM_SIZE:
		ResizeExplorerView(hwnd, param);
		return TRUE;
	case WM_CONTEXTMENU:
		return LGitContextMenuFromSubmenu(hwnd, param->menu, 1, LOWORD(lParam), HIWORD(lParam));
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_EXPLORER_REPOSITORY_CLOSE:
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		default:
			/* we might have a LOT of commands here */
			return HandleExplorerCommand(hwnd, LOWORD(wParam), param);
		}
	case WM_DROPFILES:
		/*
		 * XXX: Should we use COM drop? It's more flexible per
		 * https://www.codeproject.com/Articles/840/How-to-Implement-Drag-and-Drop-Between-Your-Progra
		 * but may be annoying from our C-flavour C++
		 */
		if (!param->ctx->active) {
			return TRUE;
		}
		LGitLog(" ! Accepting drop\n");
		if (LGitStageDragTarget(param->ctx, hwnd, (HDROP)wParam) == SCC_OK) {
			FillExplorerListView(hwnd, param);
			UpdateExplorerMenu(hwnd, param);
		}
		return TRUE;
	case WM_NOTIFY:
		switch (wParam) {
		case IDC_EXPLORER_FILES:
			LPNMHDR child_msg = (LPNMHDR)lParam;
			switch (child_msg->code) {
			case LVN_ITEMACTIVATE:
				SelectedFileProperties(hwnd, param);
				return TRUE;
			case LVN_ITEMCHANGED:
				UpdateExplorerMenu(hwnd, param);
				return TRUE;
			}
		}
		return FALSE;
	case WM_DESTROY:
		DestroyWindow(param->status_bar);
		/* no need to disassociate the SIL if the style is set to share */
		return TRUE;
	default:
		return FALSE;
	}
}

static SCCRTN LGitExplorer(LGitContext *ctx,
						   HWND hWnd,
						   LONG nFiles,
						   LPCSTR* lpFileNames,
						   BOOL standalone)
{
	int i, path_count = 0;
	LGitLog("  files %d\n", nFiles);
	LGitLog("  standalone %d\n", standalone);
	std::set<std::string> initial_select;
	for (i = 0; i < nFiles; i++) {
		char *path;
		const char *raw_path = LGitStripBasePath(ctx, lpFileNames[i]);
		if (raw_path == NULL) {
			LGitLog("    Couldn't get base path for %s\n", lpFileNames[i]);
			continue;
		}
		/* Translate because libgit2 operates with forward slashes */
		path = strdup(raw_path);
		LGitTranslateStringChars(path, '\\', '/');
		LGitLog("    %s\n", path);
		initial_select.insert(path);
	}
	LGitExplorerParams params;
	params.ctx = ctx;
	params.menu = LoadMenu(ctx->dllInst, MAKEINTRESOURCE(IDR_EXPLORER_MENU));
	params.initial_select = &initial_select;
	params.standalone = standalone;
	/* initial view state */
	params.include_ignored = FALSE;
	params.include_unmodified = FALSE;
	params.include_untracked = TRUE;
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_EXPLORER),
		hWnd,
		ExplorerDialogProc,
		(LPARAM)&params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		break;
	default:
		break;
	}
	/* XXX: Persist changes made by the user in the menus */
	DestroyMenu(params.menu);
	return SCC_OK;
}

SCCRTN SccRunScc(LPVOID context, 
				 HWND hWnd, 
				 LONG nFiles, 
				 LPCSTR* lpFileNames)
{
	LGitContext *ctx = (LGitContext*)context;
	LGitLog("**SccRunScc** Context=%p\n", context);
	return LGitExplorer(ctx, hWnd, nFiles, lpFileNames, FALSE);
}

LGIT_API SCCRTN LGitStandaloneExplorer(LGitContext *ctx, 
									   LONG nFiles, 
									   LPCSTR* lpFileNames) {
	LGitLog("**LGitStandaloneExplorer** Context=%p\n", ctx);
	return LGitExplorer(ctx, NULL, nFiles, lpFileNames, TRUE);
}