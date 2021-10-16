/*
 * Remote management and the dialog.
 */

#include "stdafx.h"

typedef struct _LGitEditRemoteDialogParams {
	LGitContext *ctx;
	BOOL is_new;
	char new_name[128];
	char new_url[128];
	char new_push_url[128];
} LGitEditRemoteDialogParams;

static void InitRemoteEditView(HWND hwnd, LGitEditRemoteDialogParams* params)
{
	wchar_t buf[128];
	LGitUtf8ToWide(params->new_name, buf, 128);
	SetDlgItemTextW(hwnd, IDC_REMOTE_EDIT_NAME, buf);
	if (!params->is_new) {
		SendDlgItemMessage(hwnd, IDC_REMOTE_EDIT_NAME, EM_SETREADONLY, TRUE, 0);
	}
	LGitUtf8ToWide(params->new_url, buf, 128);
	SetDlgItemTextW(hwnd, IDC_REMOTE_EDIT_URL, buf);
	LGitUtf8ToWide(params->new_push_url, buf, 128);
	SetDlgItemTextW(hwnd, IDC_REMOTE_EDIT_PUSHURL, buf);
}

static SetRemoteEditParams(HWND hwnd, LGitEditRemoteDialogParams* params)
{
	wchar_t buf[128];
	if (params->is_new) {
		GetDlgItemTextW(hwnd, IDC_REMOTE_EDIT_NAME, buf, 128);
		LGitWideToUtf8(buf, params->new_name, 128);
	}
	GetDlgItemTextW(hwnd, IDC_REMOTE_EDIT_URL, buf, 128);
	LGitWideToUtf8(buf, params->new_url, 128);
	GetDlgItemTextW(hwnd, IDC_REMOTE_EDIT_PUSHURL, buf, 128);
	LGitWideToUtf8(buf, params->new_push_url, 128);
}

static BOOL CALLBACK RemoteEditorDialogProc(HWND hwnd,
											unsigned int iMsg,
											WPARAM wParam,
											LPARAM lParam)
{
	LGitEditRemoteDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitEditRemoteDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitRemoteEditView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitEditRemoteDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			SetRemoteEditParams(hwnd, param);
			EndDialog(hwnd, 2);
			return TRUE;
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		}
		return FALSE;
	default:
		return FALSE;
	}
}

typedef struct _LGitRemoteDialogParams {
	LGitContext *ctx;
	/* for label editor */
	wchar_t old_name[128];
	BOOL editing;
} LGitRemoteDialogParams;

static LVCOLUMN name_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 75, "Name"
};

static LVCOLUMN url_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 250, "URL"
};

static LVCOLUMN pushurl_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 225, "Push URL"
};

static void InitRemoteView(HWND hwnd, LGitRemoteDialogParams* params)
{
	HWND lv = GetDlgItem(hwnd, IDC_REMOTE_LIST);
	ListView_SetUnicodeFormat(lv, TRUE);
	SendMessage(lv, WM_SETFONT, (WPARAM)params->ctx->listviewFont, TRUE);

	ListView_InsertColumn(lv, 0, &name_column);
	ListView_InsertColumn(lv, 1, &url_column);
	ListView_InsertColumn(lv, 2, &pushurl_column);
}

static void FillRemoteView(HWND hwnd, LGitRemoteDialogParams* params)
{
	HWND lv = GetDlgItem(hwnd, IDC_REMOTE_LIST);
	size_t i, index = 0;
	/* clear if we're replenishing */
	ListView_DeleteAllItems(lv);
	/* similar to the one in winutils.cpp */
	git_strarray remotes;
	ZeroMemory(&remotes, sizeof(git_strarray));
	if (git_remote_list(&remotes, params->ctx->repo) != 0) {
		LGitLibraryError(hwnd, "git_remote_list");
		return;
	}
	LGitLog(" ! Got back %d remote(s)\n", remotes.count);
	for (i = 0; i < remotes.count; i++) {
		const char *name = remotes.strings[i];
		LGitLog(" ! Adding remote %s\n", name);
		/* our use for the remote object is short-lived */
		git_remote *remote = NULL;
		const char *url = NULL, *push_url = NULL;
		/* in theory we could check for not found/invalid spec, but... */
		if (git_remote_lookup(&remote, params->ctx->repo, name) != 0) {
			LGitLibraryError(hwnd, "git_remote_list");
			continue;
		}
		url = git_remote_url(remote);
		if (url == NULL) {
			url = "";
		}
		push_url = git_remote_pushurl(remote);
		if (push_url == NULL) {
			push_url = "";
		}
		wchar_t buf[128];
		LVITEMW lvi;
		
		ZeroMemory(&lvi, sizeof(LVITEMW));
		lvi.mask = LVIF_TEXT;
		LGitUtf8ToWide(name, buf, 128);
		lvi.pszText = buf;
		lvi.iItem = index++;
		lvi.iSubItem = 0;

		lvi.iItem = SendMessage(lv, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
		if (lvi.iItem == -1) {
			LGitLog(" ! ListView_InsertItem failed\n");
			goto inner_fail;
		}
		/* now for the subitems... */
		lvi.iSubItem = 1;
		LGitUtf8ToWide(url, buf, 128);
		lvi.pszText = buf;
		SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);
		
		lvi.iSubItem = 2;
		LGitUtf8ToWide(push_url, buf, 128);
		lvi.pszText = buf;
		SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);
inner_fail:
		git_remote_free(remote);
	}
	git_strarray_dispose(&remotes);
}

static BOOL GetSelectedRemote(HWND hwnd, char *buf, size_t bufsz)
{
	HWND lv = GetDlgItem(hwnd, IDC_REMOTE_LIST);
	if (lv == NULL) {
		return FALSE;
	}
	int selected = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
	if (selected == -1) {
		return FALSE;
	}
	wchar_t temp_buf[1024];
	LVITEMW lvi;
	ZeroMemory(&lvi, sizeof(lvi));
	lvi.mask = LVIF_TEXT;
	lvi.iItem = selected;
	lvi.iSubItem = 0;
	lvi.pszText = temp_buf;
	lvi.cchTextMax = 1024;
	if (SendMessage(lv, LVM_GETITEMTEXTW, selected, (LPARAM)&lvi) < 1) {
		return FALSE;
	}
	LGitWideToUtf8(temp_buf, buf, bufsz);
	return TRUE;
}

static void RemoteAdd(HWND hwnd, LGitRemoteDialogParams* params)
{
	LGitEditRemoteDialogParams er_params;
	ZeroMemory(&er_params, sizeof(LGitEditRemoteDialogParams));
	er_params.ctx = params->ctx;
	er_params.is_new = TRUE;
	switch (DialogBoxParamW(params->ctx->dllInst,
		MAKEINTRESOURCEW(IDD_REMOTE_EDIT),
		hwnd,
		RemoteEditorDialogProc,
		(LPARAM)&er_params)) {
	case 0:
		LGitLog(" ! Uh-oh, dialog error\n");
		return;
	case 1:
		return;
	case 2:
		break;
	}
	git_remote *remote;
	switch (git_remote_create(&remote, params->ctx->repo, er_params.new_name, er_params.new_url)) {
	case 0:
		break;
	case GIT_EINVALIDSPEC:
		MessageBox(hwnd,
			"The remote has an invalid name.",
			"Invalid Remote",
			MB_ICONERROR);
		return;
	case GIT_EEXISTS:
		MessageBox(hwnd,
			"The remote by that name already exists.",
			"Invalid Remote",
			MB_ICONERROR);
		return;
	default:
		LGitLibraryError(hwnd, "git_remote_create");
		return;
	}
	if (strlen(er_params.new_push_url) > 0) {
		if (git_remote_set_pushurl(params->ctx->repo, er_params.new_name, er_params.new_push_url) != 0) {
			LGitLibraryError(hwnd, "git_remote_set_pushurl");
			goto fin;
		}
	}
	/* Optimization would be adding the list view item */
	FillRemoteView(hwnd, params);
fin:
	if (remote != NULL) {
		git_remote_free(remote);
	}
}

static void RemoteEdit(HWND hwnd, LGitRemoteDialogParams* params)
{
	char name[128];
	if (!GetSelectedRemote(hwnd, name, 128)) {
		LGitLog(" ! No remote?\n");
		return;
	}
	LGitLog(" ! Editing %s?\n", name);
	git_remote *remote = NULL;
	if (git_remote_lookup(&remote, params->ctx->repo, name) != 0) {
		LGitLibraryError(hwnd, "git_remote_lookup");
		return;
	}
	LGitEditRemoteDialogParams er_params;
	ZeroMemory(&er_params, sizeof(LGitEditRemoteDialogParams));
	er_params.ctx = params->ctx;
	er_params.is_new = FALSE;
	strlcpy(er_params.new_name, name, 128);
	const char *old_url = git_remote_url(remote);
	strlcpy(er_params.new_url, old_url == NULL ? "" : old_url, 128);
	const char *old_push_url = git_remote_pushurl(remote);
	strlcpy(er_params.new_push_url, old_push_url == NULL ? "" : old_push_url, 128);
	switch (DialogBoxParamW(params->ctx->dllInst,
		MAKEINTRESOURCEW(IDD_REMOTE_EDIT),
		hwnd,
		RemoteEditorDialogProc,
		(LPARAM)&er_params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		goto fin;
	case 1:
		goto fin;
	case 2:
		break;
	}
	if (strlen(er_params.new_url) > 0) {
		if (git_remote_set_url(params->ctx->repo, name, er_params.new_url) != 0) {
			LGitLibraryError(hwnd, "git_remote_set_url");
			goto fin;
		}
	}
	if (strlen(er_params.new_push_url) > 0) {
		if (git_remote_set_pushurl(params->ctx->repo, name, er_params.new_push_url) != 0) {
			LGitLibraryError(hwnd, "git_remote_set_pushurl");
			goto fin;
		}
	}
	/* Optimization would be editing the list view subitems. */
	FillRemoteView(hwnd, params);
fin:
	if (remote != NULL) {
		git_remote_free(remote);
	}
}

static void RemoveRemote(HWND hwnd, LGitRemoteDialogParams* params)
{
	char name[128];
	if (!GetSelectedRemote(hwnd, name, 128)) {
		LGitLog(" ! No remote?\n");
		return;
	}
	LGitLog(" ! Removing %s?\n", name);
	if (MessageBox(hwnd,
		"All remote tracking branches and settings for the remote will be removed.",
		"Remove Remote?",
		MB_ICONWARNING | MB_YESNO) != IDYES) {
		return;
	}
	if (git_remote_delete(params->ctx->repo, name) != 0) {
		LGitLibraryError(hwnd, "git_remote_delete");
		return;
	}
	/* Optimization would be removing the list view item */
	FillRemoteView(hwnd, params);
}

static BOOL BeginRemoteRename(HWND hwnd, LGitRemoteDialogParams* params, UINT index)
{
	HWND lv = GetDlgItem(hwnd, IDC_REMOTE_LIST);
	if (lv == NULL) {
		return FALSE;
	}
	LGitLog(" ! Begin rename for %u\n", index);
	LVITEMW lvi;
	ZeroMemory(&lvi, sizeof(lvi));
	lvi.mask = LVIF_TEXT;
	lvi.iItem = index;
	lvi.iSubItem = 0;
	lvi.pszText = params->old_name;
	lvi.cchTextMax = 128;
	if (SendMessage(lv, LVM_GETITEMTEXTW, index, (LPARAM)&lvi) < 1) {
		LGitLog("!! Failed to get rename text for %u\n", index);
		return FALSE;
	}
	params->editing = TRUE;
	return TRUE;
}

static BOOL RemoteRename(HWND hwnd, LGitRemoteDialogParams* params, const wchar_t *new_name_utf16)
{
	LGitLog(" ! Renaming %S to %S\n", params->old_name, new_name_utf16);
	params->editing = FALSE;
	/* Check if it's the same name or null; lg2 will throw an error if so. */
	if (new_name_utf16 == NULL || wcscmp(params->old_name, new_name_utf16) == 0) {
		return FALSE;
	}
	/* keep semantics */
	char new_name[128], old_name[128];
	LGitWideToUtf8(new_name_utf16, new_name, 128);
	LGitWideToUtf8(params->old_name, old_name, 128);
	/*
	 * This is slightly awkward because we have the old name in params because
	 * of how the ListView label edit messages are structured.
	 */
	git_strarray problems = {0,0};
	switch (git_remote_rename(&problems, params->ctx->repo, old_name, new_name)) {
	case 0:
		/* XXX: Report problems in their own view. For now, log */
		if (problems.count > 0) {
			for (size_t i = 0; i < problems.count; i++) {
				LGitLog(" ! Problem renaming %s\n", problems.strings[i]);
			}
			MessageBox(hwnd,
				"The remote was renamed successfully, but some refspecs couldn't be renamed.",
				"Problems Renaming",
			MB_ICONERROR);
		}
		git_strarray_dispose(&problems);
		/* Optimization would be replacing the list view item label */
		FillRemoteView(hwnd, params);
		return TRUE;
		/* XXX: These messages are common with New */
	case GIT_EINVALIDSPEC:
		MessageBox(hwnd,
			"The remote has an invalid name.",
			"Invalid Remote",
			MB_ICONERROR);
		return FALSE;
	case GIT_EEXISTS:
		MessageBox(hwnd,
			"The remote by that name already exists.",
			"Invalid Remote",
			MB_ICONERROR);
		return FALSE;
	default:
		LGitLibraryError(hwnd, "git_remote_rename");
		return FALSE;
	}
}

static BOOL CALLBACK RemoteManagerDialogProc(HWND hwnd,
											 unsigned int iMsg,
											 WPARAM wParam,
											 LPARAM lParam)
{
	LGitRemoteDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitRemoteDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitRemoteView(hwnd, param);
		FillRemoteView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitRemoteDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		if (param->editing) {
			/* so editing in the label won't close the dialog */
			return TRUE;
		}
		switch (LOWORD(wParam)) {
		case IDC_REMOTE_SETURL:
			RemoteEdit(hwnd, param);
			return TRUE;
		case IDC_REMOTE_DELETE:
			RemoveRemote(hwnd, param);
			return TRUE;
		case IDC_REMOTE_ADD:
			RemoteAdd(hwnd, param);
			return TRUE;
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		}
		return FALSE;
	case WM_NOTIFY:
		param = (LGitRemoteDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (wParam) {
		case IDC_REMOTE_LIST:
			LPNMHDR child_msg = (LPNMHDR)lParam;
			LPNMITEMACTIVATE child_activate = (LPNMITEMACTIVATE)lParam;
			NMLVDISPINFOW *child_edit = (NMLVDISPINFOW*)lParam;
			HWND lv = (HWND)wParam;
			switch (child_msg->code) {
			case LVN_ITEMACTIVATE:
				ListView_EditLabel(lv, child_activate->iItem);
				return TRUE;
			case LVN_BEGINLABELEDITW:
				return BeginRemoteRename(hwnd, param, child_edit->item.iItem);
			case LVN_ENDLABELEDITW:
				return RemoteRename(hwnd, param, child_edit->item.pszText);
			case LVN_ITEMCHANGED:
				return TRUE;
			}
		}
		return FALSE;
	default:
		return FALSE;
	}
}

SCCRTN LGitShowRemoteManager(LGitContext *ctx, HWND hwnd)
{
	LGitLog("**LGitShowRemoteManager** Context=%p\n", ctx);
	LGitRemoteDialogParams params;
	ZeroMemory(&params, sizeof(LGitRemoteDialogParams));
	params.ctx = ctx;
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_REMOTES),
		hwnd,
		RemoteManagerDialogProc,
		(LPARAM)&params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		return SCC_E_UNKNOWNERROR;
	default:
		break;
	}
	return SCC_OK;
}