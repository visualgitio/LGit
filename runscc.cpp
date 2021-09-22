/*
 * The SccRunScc/"Explorer" UI that does everything the IDE can't.
 */

#include "stdafx.h"

typedef struct _LGitExplorerParams {
	LGitContext *ctx;
	HMENU menu;
	std::set<std::string> *initial_select;
} LGitExplorerParams;

/* put here for convenience */
static LVCOLUMN name_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 400, "Name"
};
static LVCOLUMN status_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 125, "Status"
};

static void InitExplorerListView(HWND hwnd, LGitExplorerParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_EXPLORER_FILES);

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

static BOOL StatusToString(unsigned int flags, char *buf, size_t bufsz)
{
	if (bufsz < 1) {
		return FALSE;
	}
	int index = 0;
	buf[0] = '\0';
	/* now for the flags */
#define AppendIfStatus(flag,str) if (flags & flag) { if (index++) strlcat(buf, ", ", bufsz); strlcat(buf, str, bufsz); }
	AppendIfStatus(GIT_STATUS_INDEX_NEW, "New in stage");
	AppendIfStatus(GIT_STATUS_INDEX_MODIFIED, "Changed in stage");
	AppendIfStatus(GIT_STATUS_INDEX_DELETED, "Deleted from stage");
	AppendIfStatus(GIT_STATUS_INDEX_TYPECHANGE, "Type changed in stage");
	AppendIfStatus(GIT_STATUS_WT_NEW, "New");
	AppendIfStatus(GIT_STATUS_WT_MODIFIED, "Changed");
	AppendIfStatus(GIT_STATUS_WT_DELETED, "Deleted");
	AppendIfStatus(GIT_STATUS_WT_TYPECHANGE, "Type changed");
	AppendIfStatus(GIT_STATUS_WT_RENAMED, "Renaming");
	AppendIfStatus(GIT_STATUS_WT_UNREADABLE, "Unreadable");
	AppendIfStatus(GIT_STATUS_IGNORED, "Ignored");
	AppendIfStatus(GIT_STATUS_CONFLICTED, "Conflicting");
	return TRUE;
}

static int FillStatusItem(const char *relative_path,
						  unsigned int flags,
						  void *context)
{
	LVITEM lvi;

	LGitExplorerStatusCallbackParams *params = (LGitExplorerStatusCallbackParams*)context;

	/* Get the system image list index for the file, needing the full path */
	SHFILEINFO sfi;
	ZeroMemory(&sfi, sizeof(sfi));
	char path[2048];
	strlcpy(path, params->ctx->workdir_path, 2048);
	strlcat(path, relative_path, 2048);
	LGitTranslateStringChars(path, '/', '\\');
	/* XXX: Should this be cached i.e. by extension? */
	SHGetFileInfo(path, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);

	/*
	 * Unlike the equivalent in query.cpp, we can just turn a libgit2 flags
	 * field into a string.
	 */
	ZeroMemory(&lvi, sizeof(LVITEM));
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
	lvi.pszText = (char*)relative_path;
	lvi.iImage = sfi.iIcon;
	if (params->initial_select->count(relative_path)) {
		lvi.state = LVIS_SELECTED;
	}
	lvi.iItem = params->index;
	lvi.iSubItem = 0;
	lvi.iItem = ListView_InsertItem(params->lv, &lvi);
	if (lvi.iItem == -1) {
		LGitLog(" ! ListView_InsertItem failed\n");
		return GIT_EUSER;
	}
	
	char msg[256];
	StatusToString(flags, msg, 256);
	lvi.mask = LVIF_TEXT;
	lvi.pszText = (char*)msg;
	lvi.iItem = params->index;
	lvi.iSubItem = 1;
	ListView_SetItem(params->lv, &lvi);

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
	sopts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED
		| GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS
		| GIT_STATUS_OPT_INCLUDE_UNREADABLE
		| GIT_STATUS_OPT_INCLUDE_UNMODIFIED;

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

static void RefreshExplorer(HWND hwnd, LGitExplorerParams *params)
{
	char newTitle[512];
	if (params->ctx->active && params->ctx->repo != NULL) {
		char head[64], state_msg[32];
		int state = git_repository_state(params->ctx->repo);
		if (state != GIT_REPOSITORY_STATE_NONE) {
			strlcpy(state_msg, ", ", 32);
			strlcat(state_msg, LGitRepoStateString(state), 32);
		} else {
			strlcpy(state_msg, "", 32);
		}
		GetHeadState(hwnd, params, head, 64);
		_snprintf(newTitle, 512,
			"(%s%s) %s",
			head,
			state_msg,
			params->ctx->workdir_path);
	} else {
		strlcpy(newTitle, "(no repo)", 512);
	}
	SetWindowText(hwnd, newTitle);
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
	ListView_GetItemText(lv, selected, 0, buf, bufsz);
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
	while ((selected = ListView_GetNextItem(lv, selected, LVNI_SELECTED)) != -1) {
		/* XXX: measure somehow */
		char *buf = (char*)malloc(1024);
		ListView_GetItemText(lv, selected, 0, buf, 1024);
		strings[index++] = buf;
	}
	return TRUE;
}

static BOOL HandleExplorerCommand(HWND hwnd, UINT cmd, LGitExplorerParams *params)
{
	git_strarray strings;
	int ret;
	switch (cmd) {
	case ID_EXPLORER_REPOSITORY_REFRESH:
		RefreshExplorer(hwnd, params);
		FillExplorerListView(hwnd, params);
		return TRUE;
	case ID_EXPLORER_REPOSITORY_DIFFFROMSTAGE:
		LGitDiffStageToWorkdir(params->ctx, hwnd);
		return TRUE;
	case ID_EXPLORER_REPOSITORY_APPLYPATCH:
		if (LGitApplyPatchDialog(params->ctx, hwnd) == SCC_OK) {
			RefreshExplorer(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_REPOSITORY_BRANCHES:
		LGitShowBranchManager(params->ctx, hwnd);
		/* operations here can cause i.e. checkouts */
		RefreshExplorer(hwnd, params);
		return TRUE;
	case ID_EXPLORER_REPOSITORY_HISTORY:
		SccHistory(params->ctx, hwnd, 0, NULL, 0, NULL);
		return TRUE;
	case ID_EXPLORER_STAGE_ADDFILES:
		if (LGitStageAddDialog(params->ctx, hwnd) == SCC_OK) {
			FillExplorerListView(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_UPDATE:
		if (GetMultipleSelection(hwnd, &strings)) {
			LGitStageAddFiles(params->ctx, hwnd, &strings, TRUE);
			LGitFreePathList(strings.strings, strings.count);
			FillExplorerListView(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_REMOVE:
		ret = MessageBox(hwnd,
			"The files will no longer be tracked by Git. Are you sure?",
			"Really Remove?",
			MB_ICONWARNING | MB_YESNO);
		if (ret != IDYES) {
			return TRUE;
		}
		if (GetMultipleSelection(hwnd, &strings)) {
			LGitStageRemoveFiles(params->ctx, hwnd, &strings);
			LGitFreePathList(strings.strings, strings.count);
			FillExplorerListView(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_UNSTAGE:
		/* XXX: make into a question? */
		if (GetMultipleSelection(hwnd, &strings)) {
			LGitStageUnstageFiles(params->ctx, hwnd, &strings);
			LGitFreePathList(strings.strings, strings.count);
			FillExplorerListView(hwnd, params);
		}
		return TRUE;
	case ID_EXPLORER_STAGE_REVERTTOSTAGED:
		/* XXX: make into a question? */
		if (GetMultipleSelection(hwnd, &strings)) {
			LGitCheckoutStaged(params->ctx, hwnd, &strings);
			LGitFreePathList(strings.strings, strings.count);
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
	case ID_EXPLORER_HELP_ABOUT:
		LGitAbout(hwnd, params->ctx);
		return TRUE;
	default:
		return FALSE;
	}
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
	/* Because we can be invoked without a valid repo... */
	UINT newState = MF_IF_CMD(active);
	UINT newStateSelected = MF_IF_CMD(active && selected > 0);
#define EnableMenuItemIfInRepo(id) EnableMenuItem(params->menu,id,newState)
#define EnableMenuItemIfSelected(id) EnableMenuItem(params->menu,id,newStateSelected)
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_REFRESH);
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_DIFFFROMSTAGE);
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_APPLYPATCH);
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_BRANCHES);
	EnableMenuItemIfInRepo(ID_EXPLORER_REPOSITORY_HISTORY);
	EnableMenuItemIfInRepo(ID_EXPLORER_STAGE_ADDFILES);
	EnableMenuItemIfSelected(ID_EXPLORER_STAGE_UPDATE);
	EnableMenuItemIfSelected(ID_EXPLORER_STAGE_REMOVE);
	EnableMenuItemIfSelected(ID_EXPLORER_STAGE_UNSTAGE);
	EnableMenuItemIfSelected(ID_EXPLORER_STAGE_REVERTTOSTAGED);
	EnableMenuItemIfInRepo(ID_EXPLORER_STAGE_COMMITSTAGED);
	EnableMenuItemIfInRepo(ID_EXPLORER_REMOTE_MANAGEREMOTES);
	EnableMenuItemIfInRepo(ID_EXPLORER_REMOTE_PUSH);
	EnableMenuItemIfInRepo(ID_EXPLORER_REMOTE_PULL);
	EnableMenuItemIfInRepo(ID_EXPLORER_CONFIG_REPOSITORY);
}

static void InitExplorerView(HWND hwnd, LGitExplorerParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_EXPLORER_FILES);
	LGitSetWindowIcon(hwnd, params->ctx->dllInst, MAKEINTRESOURCE(IDI_LGIT));
	SetMenu(hwnd, params->menu);
	RefreshExplorer(hwnd, params);
	UpdateExplorerMenu(hwnd, params);
	if (params->ctx->repo == NULL) {
		/* We're gonna do stuff we can only do with i.e. a stage */
		ListView_DeleteAllItems(lv);
		EnableWindow(lv, FALSE);
		return;
	}
	EnableWindow(lv, TRUE);
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
		InitExplorerListView(hwnd, param);
		InitExplorerView(hwnd, param);
		LGitControlFillsParentDialog(hwnd, IDC_EXPLORER_FILES);
		FillExplorerListView(hwnd, param);
		/* empty the selection that we no longer need it */
		param->initial_select->clear();
		return TRUE;
	case WM_SIZE:
		LGitControlFillsParentDialog(hwnd, IDC_EXPLORER_FILES);
		return TRUE;
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
		{
			/* annoying dtor we have to do, or the SIL gets blown away */
			HWND lv = GetDlgItem(hwnd, IDC_EXPLORER_FILES);
			ListView_SetImageList(lv, NULL, LVSIL_SMALL);
		}
		return TRUE;
	default:
		return FALSE;
	}
}

SCCRTN SccRunScc(LPVOID context, 
				 HWND hWnd, 
				 LONG nFiles, 
				 LPCSTR* lpFileNames)
{
	int i, path_count = 0;
	LGitContext *ctx = (LGitContext*)context;
	LGitLog("**SccRunScc** Context=%p\n", context);
	LGitLog("  files %d\n", nFiles);
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
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_EXPLORER),
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
	DestroyMenu(params.menu);
	return SCC_OK;
}
