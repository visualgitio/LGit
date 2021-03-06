/*
 * The diff window, broken out because it's so big.
 */

#include "stdafx.h"

#pragma comment(lib, "comctl32")

static LVCOLUMN diff_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 200, "Diff Line"
};

typedef struct _LGitDiffCallbackParams {
	HWND lv;
	int index;
	/* allocated to avoid stack churn/stupid line lengths */
	char *msg;
	wchar_t *msgw;
} LGitDiffCallbackParams;

#define CALLBACK_MSG_SIZE 0x8000

static void SetDiffTitleBar(HWND hwnd, LGitDiffDialogParams* params)
{
	if (params->path) {
		char title[256];
		_snprintf(title, 256, "Diff for %s", params->path);
		SetWindowText(hwnd, title);
	}
	/* default otherwise */
}

static void InitDiffView(HWND hwnd, LGitDiffDialogParams* params)
{
	SetMenu(hwnd, params->menu);

	HWND lv;
	/* XXX: It's unclear if we need to free this. */
	HIMAGELIST icons;
	HICON icon;

	lv = GetDlgItem(hwnd, IDC_DIFFTEXT);
	ListView_SetUnicodeFormat(lv, TRUE);

	icons = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		ILC_MASK, 1, 1);
	icon = LoadIcon(params->ctx->dllInst, MAKEINTRESOURCE(IDI_DIFF_FILE_A));
	ImageList_AddIcon(icons, icon);
	DestroyIcon(icon);
	icon = LoadIcon(params->ctx->dllInst, MAKEINTRESOURCE(IDI_DIFF_FILE_B));
	ImageList_AddIcon(icons, icon);
	DestroyIcon(icon);
	icon = LoadIcon(params->ctx->dllInst, MAKEINTRESOURCE(IDI_DIFF_BINARY));
	ImageList_AddIcon(icons, icon);
	DestroyIcon(icon);
	icon = LoadIcon(params->ctx->dllInst, MAKEINTRESOURCE(IDI_DIFF_HUNK));
	ImageList_AddIcon(icons, icon);
	DestroyIcon(icon);
	icon = LoadIcon(params->ctx->dllInst, MAKEINTRESOURCE(IDI_DIFF_ADD));
	ImageList_AddIcon(icons, icon);
	DestroyIcon(icon);
	icon = LoadIcon(params->ctx->dllInst, MAKEINTRESOURCE(IDI_DIFF_DEL));
	ImageList_AddIcon(icons, icon);
	DestroyIcon(icon);

	ListView_SetImageList(lv, icons, LVSIL_SMALL);
	ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT
		| LVS_EX_HEADERDRAGDROP
		| LVS_EX_LABELTIP);

	ListView_InsertColumn(lv, 0, &diff_column);

	LGitSetMonospaceFont(params->ctx, lv);
}

static int LGitDiffFileCallback(const git_diff_delta *delta,
								float progress,
								void *payload)
{
	LGitDiffCallbackParams *params = (LGitDiffCallbackParams*)payload;
	LVITEMW lvi;
	wchar_t path[2048];
	ZeroMemory(&lvi, sizeof(LVITEMW));
	lvi.iItem = params->index++;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;
	/* XXX: We should detect similarity and just skim the diff if so. */
	LGitUtf8ToWide(delta->old_file.path, path, 2048);
	_snwprintf(params->msgw, CALLBACK_MSG_SIZE, L"(%o) %s",
		delta->old_file.mode,
		path);
	lvi.pszText = params->msgw;
	lvi.iSubItem = 0;
	lvi.iImage = 0;

	lvi.iItem = SendMessage(params->lv, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
	if (lvi.iItem == -1) {
		LGitLog(" ! ListView_InsertItem failed for file A\n");
		return 1;
	}
	/* Now item B */
	ZeroMemory(&lvi, sizeof(LVITEMW));
	lvi.iItem = params->index++;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;
	LGitUtf8ToWide(delta->new_file.path, path, 2048);
	_snwprintf(params->msgw, CALLBACK_MSG_SIZE, L"(%o) %s",
		delta->new_file.mode,
		path);
	lvi.pszText = params->msgw;
	lvi.iSubItem = 0;
	lvi.iImage = 1;

	lvi.iItem = SendMessage(params->lv, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
	if (lvi.iItem == -1) {
		LGitLog(" ! ListView_InsertItem failed for file B\n");
		return 1;
	}
	return 0;
}

static int LGitDiffBinaryCallback(const git_diff_delta *delta,
								  const git_diff_binary *binary,
								  void *payload)
{
	LGitDiffCallbackParams *params = (LGitDiffCallbackParams*)payload;
	LVITEM lvi;
	ZeroMemory(&lvi, sizeof(LVITEM));
	lvi.iItem = params->index++;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;
	lvi.pszText = "Binary file differs";
	lvi.iSubItem = 0;
	lvi.iImage = 2;

	lvi.iItem = ListView_InsertItem(params->lv, &lvi);
	if (lvi.iItem == -1) {
		LGitLog(" ! ListView_InsertItem failed for binary\n");
		return 1;
	}
	return 0;
}

static int LGitDiffHunkCallback(const git_diff_delta *delta,
								const git_diff_hunk *hunk,
								void *payload)
{
	LGitDiffCallbackParams *params = (LGitDiffCallbackParams*)payload;
	LVITEMW lvi;
	ZeroMemory(&lvi, sizeof(LVITEMW));
	lvi.iItem = params->index++;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;

	strlcpy(params->msg, hunk->header, CALLBACK_MSG_SIZE);
	size_t length = strlen(params->msg);
	if (length > 1 && !isprint(params->msg[length - 1])) {
		params->msg[length - 1] = '\0';
	}
	if (length > 2 && !isprint(params->msg[length - 2])) {
		params->msg[length - 2] = '\0';
	}
	LGitUtf8ToWide(params->msg, params->msgw, CALLBACK_MSG_SIZE);
	lvi.pszText = params->msgw;

	lvi.iSubItem = 0;
	lvi.iImage = 3;

	lvi.iItem = SendMessage(params->lv, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
	if (lvi.iItem == -1) {
		LGitLog(" ! ListView_InsertItem failed for hunk\n");
		return 1;
	}
	return 0;
}

static int LGitDiffLineCallback(const git_diff_delta *delta,
								const git_diff_hunk *hunk,
								const git_diff_line *line,
								void *payload)
{
	LGitDiffCallbackParams *params = (LGitDiffCallbackParams*)payload;
	LVITEMW lvi;
	size_t base_indent, i, length;
	ZeroMemory(&lvi, sizeof(LVITEMW));
	lvi.iItem = params->index++;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;
	if (//line->origin == GIT_DIFF_LINE_CONTEXT ||
		line->origin == GIT_DIFF_LINE_ADDITION ||
		line->origin == GIT_DIFF_LINE_DELETION) {
		lvi.iImage = line->origin == GIT_DIFF_LINE_ADDITION
			? 4 : 5;
	} else {
		lvi.iImage = 6; /* doesn't exist or will be nop. */
	}
#if 1
	/*
	 * This is going to be ugly because the Win32 controls don't handle tabs
	 * or newlines very well. We should render newlines when they differ, as
	 * well as any trailing spaces.
	 */
	/* First, check for tabs, then copy compensating */
	ZeroMemory(params->msg, CALLBACK_MSG_SIZE);
	for (base_indent = 0, i = 0; i < CALLBACK_MSG_SIZE && i < line->content_len; i++) {
		if (line->content[i] == '\t') {
			/* Two spaces because I say so */
			strlcat(params->msg, "  ", CALLBACK_MSG_SIZE);
			base_indent++;
		}
	}
	size_t tab_length = strlen(params->msg);
	length = __min(CALLBACK_MSG_SIZE - tab_length, line->content_len - base_indent);
	memcpy(params->msg + tab_length, line->content + base_indent, length);
	/* Truncate because not null terminated, and get newline */
	params->msg[__min(CALLBACK_MSG_SIZE - 1, length)] = '\0';
	length = strlen(params->msg);
	/* We could have a zero-length line. Could be a loop instead. */
	if (line->content_len >= 1 && !isprint(params->msg[length - 1])) {
		params->msg[length - 1] = '\0';
	}
	if (line->content_len >= 2 && !isprint(params->msg[length - 2])) {
		params->msg[length - 2] = '\0';
	}
#else
	/* Simplistic implementation */
	length = __min(CALLBACK_MSG_SIZE, line->content_len);
	memcpy(params->msg, line->content, length);
	params->msg[length] = '\0';
#endif
	LGitUtf8ToWide(params->msg, params->msgw, CALLBACK_MSG_SIZE);
	lvi.pszText = params->msgw;
	lvi.iSubItem = 0;

	lvi.iItem = SendMessage(params->lv, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
	if (lvi.iItem == -1) {
		LGitLog(" ! ListView_InsertItem failed for line\n");
		return 1;
	}
	return 0;
}

static BOOL FillDiffView(HWND hwnd, LGitDiffDialogParams* params)
{
	HWND lv;
	lv = GetDlgItem(hwnd, IDC_DIFFTEXT);
	if (lv == NULL) {
		LGitLog(" ! Couldn't get diff control\n");
		return FALSE;
	}
	LGitDiffCallbackParams cbp;
	cbp.lv = lv;
	cbp.index = 0;
	cbp.msg = (char*)malloc(CALLBACK_MSG_SIZE + 1);
	if (cbp.msg == NULL) {
		LGitLog(" ! Couldn't alloc callback buffer\n");
		return FALSE;
	}
	cbp.msgw = (wchar_t*)calloc(CALLBACK_MSG_SIZE + 1, sizeof(wchar_t));
	if (cbp.msgw == NULL) {
		free(cbp.msg);
		LGitLog(" ! Couldn't alloc wide callback buffer\n");
		return FALSE;
	}
	LGitLog(" ! Number of diff deltas: %u\n", git_diff_num_deltas(params->diff));
	git_diff_foreach(params->diff,
		LGitDiffFileCallback,
		LGitDiffBinaryCallback,
		LGitDiffHunkCallback,
		LGitDiffLineCallback,
		&cbp);
	free(cbp.msg);
	free(cbp.msgw);
	LGitLog(" ! LV Index is now %d\n", cbp.index);

	ListView_SetColumnWidth(lv, 0, LVSCW_AUTOSIZE_USEHEADER);
	return TRUE;
}

static void CopyDiff(HWND hwnd, LGitDiffDialogParams* params)
{
	git_buf blob = {0, 0};
	if (git_diff_to_buf(&blob, params->diff, GIT_DIFF_FORMAT_PATCH) != 0) {
		LGitLog("!! Couldn't put diff into buffer\n");
		return;
	}
	if (!OpenClipboard(hwnd)) {
		LGitLog("!! Couldn't open clipboard\n");
		return;
	}
	EmptyClipboard();
	/*
	 * convert to UTF-16 because CF_TEXT is ANSI
	 * XXX: It's implied 
	 */
	wchar_t *buf = LGitUtf8ToWideAlloc(blob.ptr);
	if (SetClipboardData(CF_UNICODETEXT, buf) == NULL) {
		LGitLog("!! Couldn't set clipboard\n");
	}
	CloseClipboard();
	free(buf);
	git_buf_dispose(&blob);
}

static BOOL SaveDiff(HWND hwnd,
					 LGitDiffDialogParams *params,
					 const wchar_t *fileName)
{
	/* Binary because the diff is LF, not CRLF */
	FILE *f = _wfopen(fileName, L"wb");
	if (f == NULL) {
		return FALSE;
	}
	git_buf blob = {0, 0};
	if (git_diff_to_buf(&blob, params->diff, GIT_DIFF_FORMAT_PATCH) != 0) {
		LGitLog("!! Couldn't put diff into buffer\n");
		return FALSE;
	}
	size_t written = fwrite(blob.ptr, blob.size, 1, f);
	if (written < blob.size) {
		fclose(f);
		git_buf_dispose(&blob);
		return FALSE;
	}
	fclose(f);
	git_buf_dispose(&blob);
	return TRUE;
}

static void SaveDiffDialog(HWND hwnd, LGitDiffDialogParams *params)
{
	OPENFILENAMEW ofn;
	wchar_t fileName[MAX_PATH];
	ZeroMemory(&ofn, sizeof(ofn));
	ZeroMemory(fileName, MAX_PATH);
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = L"Diff\0*.diff;*.patch\0";
	ofn.lpstrTitle = L"Save Diff";
	ofn.lpstrDefExt = L"diff";
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = MAX_PATH;
	/* For help, |= OFN_SHOWHELP | OFN_ENABLEHOOK (and hook) */
	ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
	if (GetSaveFileNameW(&ofn)) {
		SaveDiff(hwnd, params, fileName);
	}
}

static BOOL CALLBACK DiffDialogProc(HWND hwnd,
									unsigned int iMsg,
									WPARAM wParam,
									LPARAM lParam)
{
	LGitDiffDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitDiffDialogParams*)lParam;
		LGitSetWindowIcon(hwnd, param->ctx->dllInst, MAKEINTRESOURCE(IDI_DIFF));
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		SetDiffTitleBar(hwnd, param);
		InitDiffView(hwnd, param);
		if (!FillDiffView(hwnd, param)) {
			EndDialog(hwnd, 0);
		}
		LGitControlFillsParentDialog(hwnd, IDC_DIFFTEXT);
		return TRUE;
	case WM_SIZE:
		LGitControlFillsParentDialog(hwnd, IDC_DIFFTEXT);
		return TRUE;
	case WM_COMMAND:
		param = (LGitDiffDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
			/* These could copy formatted patches once we get commits here */
		case ID_DIFF_COPY:
			CopyDiff(hwnd, param);
			break;
		case ID_DIFF_SAVE:
			SaveDiffDialog(hwnd, param);
			break;
		case ID_DIFF_CLOSE:
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		}
		return FALSE;
	default:
		return FALSE;
	}
}

int LGitDiffWindow(HWND parent, LGitDiffDialogParams *params)
{
	if (params == NULL) {
		return 0;
	}
	params->menu = LoadMenu(params->ctx->dllInst, MAKEINTRESOURCE(IDR_DIFF_MENU));
	int ret = DialogBoxParamW(params->ctx->dllInst,
		MAKEINTRESOURCEW(IDD_DIFF),
		parent,
		DiffDialogProc,
		(LPARAM)params);
	DestroyMenu(params->menu);
	return ret;
}