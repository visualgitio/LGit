/*
 * Manage git config.
 */

#include "stdafx.h"

typedef struct _LGitEditConfigDialogParams {
	LGitContext *ctx;
	BOOL is_new;
	wchar_t new_name[128];
	wchar_t new_value[128];
} LGitEditConfigDialogParams;

static void InitConfigEditView(HWND hwnd, LGitEditConfigDialogParams* params)
{
	SetDlgItemTextW(hwnd, IDC_CONFIG_EDIT_NAME, params->new_name);
	HWND focus_on;
	if (params->is_new) {
		focus_on = GetDlgItem(hwnd, IDC_CONFIG_EDIT_NAME);
	} else {
		SendDlgItemMessage(hwnd, IDC_CONFIG_EDIT_NAME, EM_SETREADONLY, TRUE, 0);
		/* no point in changing the name */
		focus_on = GetDlgItem(hwnd, IDC_CONFIG_EDIT_VALUE);
	}
	SetFocus(focus_on);
	SetDlgItemTextW(hwnd, IDC_CONFIG_EDIT_VALUE, params->new_value);
}

static SetConfigEditParams(HWND hwnd, LGitEditConfigDialogParams* params)
{
	if (params->is_new) {
		GetDlgItemTextW(hwnd, IDC_CONFIG_EDIT_NAME, params->new_name, 128);
	}
	GetDlgItemTextW(hwnd, IDC_CONFIG_EDIT_VALUE, params->new_value, 128);
}

static BOOL CALLBACK ConfigEditorDialogProc(HWND hwnd,
											unsigned int iMsg,
											WPARAM wParam,
											LPARAM lParam)
{
	LGitEditConfigDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitEditConfigDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitConfigEditView(hwnd, param);
		/* NOT an error to return false, but allows changing default focus */
		return FALSE;
	case WM_COMMAND:
		param = (LGitEditConfigDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			SetConfigEditParams(hwnd, param);
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

typedef struct _LGitConfigDialogParams {
	LGitContext *ctx;
	git_config *config;
	const char *title;
} LGitConfigDialogParams;

static const wchar_t *LevelName(git_config_level_t level)
{
	switch (level) {
	case GIT_CONFIG_LEVEL_PROGRAMDATA:
		/* Windows system-wide, implied for compat only? */
		return L"System (ProgramData)";
	case GIT_CONFIG_LEVEL_SYSTEM:
		/* system-wide config */
		return L"System";
	case GIT_CONFIG_LEVEL_XDG:
		/* ~/.config/git/config; can this ever trigger on Windows? */
		return L"User (XDG)";
	case GIT_CONFIG_LEVEL_GLOBAL:
		/* User level config, usually ~/.gitconfig */
		return L"User (Global)";
	case GIT_CONFIG_LEVEL_LOCAL:
		/* Repo .git/config */
		return L"Repository";
	case GIT_CONFIG_LEVEL_APP:
		/* defined by us */
		return L"Application-Defined";
	case GIT_CONFIG_HIGHEST_LEVEL:
		/* XXX: shouldn't happen? */
		return L"Highest Level";
	default:
		return L"Unknown";
	}
}

static LVCOLUMN name_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 150, "Name"
};

static LVCOLUMN value_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 250, "Value"
};

static LVCOLUMN from_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 100, "From"
};

typedef struct _LGitConfigIterArgs {
	HWND lv;
	int index;
} LGitConfigIterArgs;

int ConfigForeach(const git_config_entry *e, void *payload)
{
	LGitConfigIterArgs *args = (LGitConfigIterArgs*)payload;
	HWND lv = args->lv;

	/*
	LGitLog("  (depth %d level %S) %s -> %s\n",
		e->include_depth,
		LevelName(e->level),
		e->name,
		e->value);
	*/

	wchar_t buf[1024];
	MultiByteToWideChar(CP_UTF8, 0, e->name, -1, buf, 1024);
	/*
	 * We should check if the item is already existant. If so, replace if the
	 * level is higher. Otherwise, it can be confusing because operations will
	 * take effect on the highest level config available.
	 */
	LVITEMW lvi;
	ZeroMemory(&lvi, sizeof(LVITEM));
	LVFINDINFOW lvfi;
	lvfi.flags = LVFI_STRING;
	lvfi.psz = buf;
	int oldIndex = ListView_FindItem(lv, -1, &lvfi);
	SendMessage(lv, LVM_FINDITEMW, -1, (LPARAM)&lvfi);
	int oldLevel = 0;
	if (oldIndex != -1) {
		lvi.iItem = oldIndex;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_PARAM;
		SendMessage(lv, LVM_GETITEMW, 0, (LPARAM)&lvi);
		oldLevel = lvi.lParam;
	}
	/* If the level is higher, update, else insert */
	ZeroMemory(&lvi, sizeof(LVITEM));
	if (oldIndex != -1 && e->level >= oldLevel) {
		lvi.iItem = oldIndex;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.pszText = buf;
		lvi.lParam = e->level;
		SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);
	} else {
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.pszText = (wchar_t*)buf;
		lvi.lParam = e->level;
		lvi.iItem = args->index++;
		lvi.iSubItem = 0;

		SendMessage(lv, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
		if (lvi.iItem == -1) {
			LGitLog(" ! ListView_InsertItem failed\n");
			return -1;
		}
	}
	/* now for the subitems... */
	MultiByteToWideChar(CP_UTF8, 0, e->value, -1, buf, 1024);
	lvi.mask = LVIF_TEXT;
	lvi.iSubItem = 1;
	lvi.pszText = (wchar_t*)buf;
	SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);

	lvi.iSubItem = 2;
	lvi.pszText = (wchar_t*)LevelName(e->level);
	SendMessage(lv, LVM_SETITEMW, 0, (LPARAM)&lvi);
	return 0;
}

static void InitConfigView(HWND hwnd, LGitConfigDialogParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_CONFIG_LIST);
	ListView_SetUnicodeFormat(lv, TRUE);

	/* I think ANSI InsertColumn is still OK with Unicode for now */
	ListView_InsertColumn(lv, 0, &name_column);
	ListView_InsertColumn(lv, 1, &value_column);
	ListView_InsertColumn(lv, 2, &from_column);

	char new_title[128];
	_snprintf(new_title, 128, "Git %s Config", params->title);
	SetWindowText(hwnd, new_title);
}

static void FillConfigView(HWND hwnd, LGitConfigDialogParams *params)
{
	HWND lv = GetDlgItem(hwnd, IDC_CONFIG_LIST);
	LGitConfigIterArgs args;
	args.lv = lv;
	args.index = 0;

	ListView_DeleteAllItems(lv);
	if (git_config_foreach(params->config, ConfigForeach, &args) != 0) {
		LGitLibraryError(hwnd, "git_config_foreach");
	}
}

static BOOL GetSelectedConfig(HWND hwnd, char *buf, size_t bufsz)
{
	HWND lv = GetDlgItem(hwnd, IDC_CONFIG_LIST);
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

/**
 * Name and value are used to pre-fill the dialog; they aren't written to.
 */
static void ConfigEditDialog(HWND hwnd, LGitConfigDialogParams *params, const char *name, const char *value, BOOL isNew)
{
	LGitEditConfigDialogParams ec_params;
	ZeroMemory(&ec_params, sizeof(LGitEditConfigDialogParams));
	ec_params.ctx = params->ctx;
	ec_params.is_new = isNew;
	MultiByteToWideChar(CP_UTF8, 0, name == NULL ? "" : name, -1, ec_params.new_name, 128);
	MultiByteToWideChar(CP_UTF8, 0, value == NULL ? "" : value, -1, ec_params.new_value, 128);
	switch (DialogBoxParam(params->ctx->dllInst,
		MAKEINTRESOURCE(IDD_CONFIG_EDIT),
		hwnd,
		ConfigEditorDialogProc,
		(LPARAM)&ec_params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		return;
	case 1:
		return;
	case 2:
		break;
	}
	if (wcslen(ec_params.new_value) > 0) {
		/* convert */
		char new_name[256];
		char new_value[256];
		WideCharToMultiByte(CP_UTF8, 0, ec_params.new_name, -1, new_name, 256, NULL, NULL);
		WideCharToMultiByte(CP_UTF8, 0, ec_params.new_value, -1, new_value, 256, NULL, NULL);
		if (git_config_set_string(params->config, new_name, new_value) != 0) {
			LGitLibraryError(hwnd, "git_config_set_string");
			return;
		}
	}
	FillConfigView(hwnd, params);
}

static void ConfigEdit(HWND hwnd, LGitConfigDialogParams *params)
{
	char name[128];
	if (!GetSelectedConfig(hwnd, name, 128)) {
		LGitLog(" ! No config?\n");
		return;
	}
	/*
	 * XXX: Check if we're editing in the same level. If not, a new value is
	 * inserted at the higher level which supersedes the lower one.
	 */
	git_buf old_val_buf = {0};
	if (git_config_get_string_buf(&old_val_buf, params->config, name) != 0) {
		LGitLibraryError(hwnd, "git_config_get_string_buf");
		return;
	}
	LGitLog(" ! Editing %s?\n", name);
	ConfigEditDialog(hwnd, params, name, old_val_buf.ptr, FALSE);
	git_buf_dispose(&old_val_buf);
}

static void ConfigRemove(HWND hwnd, LGitConfigDialogParams *params)
{
	/* XXX: Multiple selection could be handy here */
	char name[128];
	if (!GetSelectedConfig(hwnd, name, 128)) {
		LGitLog(" ! No config?\n");
		return;
	}
	LGitLog(" ! Removing %s?\n", name);
	if (MessageBox(hwnd,
		"This configuration entry will be deleted from its source. "
		"It may be replaced with an inherited configuration. Are you sure?",
		"Remove Config Entry?",
		MB_ICONWARNING | MB_YESNO) != IDYES) {
		return;
	}
	if (git_config_delete_entry(params->config, name) != 0) {
		LGitLibraryError(hwnd, "git_config_delete_entry");
		return;
	}
	/* Optimization would be removing the list view item, if not for levels */
	FillConfigView(hwnd, params);
}

static void ConfigAdd(HWND hwnd, LGitConfigDialogParams *params)
{
	ConfigEditDialog(hwnd, params, NULL, NULL, TRUE);
}

static BOOL CALLBACK ConfigManagerDialogProc(HWND hwnd,
											 unsigned int iMsg,
											 WPARAM wParam,
											 LPARAM lParam)
{
	LGitConfigDialogParams *param = (LGitConfigDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitConfigDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitConfigView(hwnd, param);
		FillConfigView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_CONFIG_SET:
			ConfigEdit(hwnd, param);
			return TRUE;
		case IDC_CONFIG_DELETE:
			ConfigRemove(hwnd, param);
			return TRUE;
		case IDC_CONFIG_ADD:
			ConfigAdd(hwnd, param);
			return TRUE;
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		}
		return FALSE;
	case WM_NOTIFY:
		switch (wParam) {
		case IDC_REMOTE_LIST:
			LPNMHDR child_msg = (LPNMHDR)lParam;
			switch (child_msg->code) {
			case LVN_ITEMACTIVATE:
				ConfigEdit(hwnd, param);
				return TRUE;
			}
		}
		return FALSE;
	default:
		return FALSE;
	}
}

SCCRTN LGitManageConfig(LGitContext *ctx, HWND hwnd, git_config *config, const char *title)
{
	LGitLog("**LGitManageConfig** Context=%p\n", ctx);
	LGitConfigDialogParams params;
	ZeroMemory(&params, sizeof(LGitConfigDialogParams));
	params.ctx = ctx;
	params.config = config;
	params.title = title;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_GITCONFIG),
		hwnd,
		ConfigManagerDialogProc,
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