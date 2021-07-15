/*
 * The diff window, broken out because it's so big.
 */

#include "stdafx.h"
#include "LGit.h"

#pragma comment(lib, "comctl32")

static LVCOLUMN diff_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 200, "Diff Line"
};

typedef struct _LGitDiffCallbackParams {
	HWND lv;
	int index;
} LGitDiffCallbackParams;

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
	HWND lv;
	/* XXX: It's unclear if we need to free this. */
	HIMAGELIST icons;
	HICON icon;
	HFONT font;

	lv = GetDlgItem(hwnd, IDC_DIFFTEXT);

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

	font = (HFONT)GetStockObject(ANSI_FIXED_FONT);

	SendMessage(lv, WM_SETFONT, (WPARAM)font, TRUE);
}

static int LGitDiffFileCallback(const git_diff_delta *delta,
								float progress,
								void *payload)
{
	LGitDiffCallbackParams *params = (LGitDiffCallbackParams*)payload;
	LVITEM lvi;
	char msg[2048];
	ZeroMemory(&lvi, sizeof(LVITEM));
	lvi.iItem = params->index++;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;
	/* XXX: We should detect similarity and just skim the diff if so. */
	_snprintf(msg, 2048, "(%o) %s",
		delta->old_file.mode,
		delta->old_file.path);
	lvi.pszText = msg;
	lvi.iSubItem = 0;
	lvi.iImage = 0;

	lvi.iItem = ListView_InsertItem(params->lv, &lvi);
	if (lvi.iItem == -1) {
		LGitLog(" ! ListView_InsertItem failed for file A\n");
		return 1;
	}
	/* Now item B */
	ZeroMemory(&lvi, sizeof(LVITEM));
	lvi.iItem = params->index++;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;
	_snprintf(msg, 2048, "(%o) %s",
		delta->new_file.mode,
		delta->new_file.path);
	lvi.pszText = msg;
	lvi.iSubItem = 0;
	lvi.iImage = 1;

	lvi.iItem = ListView_InsertItem(params->lv, &lvi);
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
	LVITEM lvi;
	ZeroMemory(&lvi, sizeof(LVITEM));
	lvi.iItem = params->index++;
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;
	lvi.pszText = (char*)hunk->header;
	lvi.iSubItem = 0;
	lvi.iImage = 3;

	lvi.iItem = ListView_InsertItem(params->lv, &lvi);
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
	LVITEM lvi;
	char msg[2048];
	size_t base_indent, i, length;
	ZeroMemory(&lvi, sizeof(LVITEM));
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
	/*
	 * This is going to be ugly because the Win32 controls don't handle tabs
	 * or newlines very well. We should render newlines when they differ, as
	 * well as any trailing spaces.
	 */
	/* First, check for tabs, then copy compensating */
	msg[0] = '\0';
	for (base_indent = 0, i = 0; i < 2048 && i < line->content_len; i++) {
		if (line->content[i] == '\t') {
			/* Two spaces because I say so */
			strcat(msg, "  ");
			base_indent++;
		}
	}
	length = __min(2048 - base_indent, line->content_len);
	memcpy(msg + (2 * base_indent), line->content + (1 * base_indent), length);
	/* Truncate because not null terminated, and get newline */
	msg[__min(2047, length)] = '\0';
	length = strlen(msg);
	if (!isprint(msg[length - 1])) {
		msg[length - 1] = '\0';
	}
	if (!isprint(msg[length - 2])) {
		msg[length - 2] = '\0';
	}
	lvi.pszText = msg;
	lvi.iSubItem = 0;

	lvi.iItem = ListView_InsertItem(params->lv, &lvi);
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
	LGitDiffCallbackParams cbp = { lv, 0 };
	if (lv == NULL) {
		LGitLog(" ! Couldn't get diff control\n");
		return FALSE;
	}
	git_diff_foreach(params->diff,
		LGitDiffFileCallback,
		LGitDiffBinaryCallback,
		LGitDiffHunkCallback,
		LGitDiffLineCallback,
		&cbp);

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
	if (SetClipboardData(CF_TEXT, blob.ptr) == NULL) {
		LGitLog("!! Couldn't set clipboard\n");
	}
	CloseClipboard();
	git_buf_dispose(&blob);
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
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		SetDiffTitleBar(hwnd, param);
		InitDiffView(hwnd, param);
		if (!FillDiffView(hwnd, param)) {
			EndDialog(hwnd, 0);
		}
		return TRUE;
	case WM_COMMAND:
		param = (LGitDiffDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDC_DIFF_COPY:
			CopyDiff(hwnd, param);
			break;
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
	return DialogBoxParam(params->ctx->dllInst,
		MAKEINTRESOURCE(IDD_DIFF),
		parent,
		DiffDialogProc,
		(LPARAM)params);
}