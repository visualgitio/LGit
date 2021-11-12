/*
 * Checkout conflict display via notifications.
 */

#include <stdafx.h>

typedef struct _LGitCheckoutNotifyResults {
	/* Can just be a list of strings. We don't use the git_diff_file* yet */
	std::set<std::string> conflicts;
} LGitCheckoutNotifyResults;

typedef struct _LGitCheckoutNotifyParams {
	LGitContext *ctx;
	LGitCheckoutNotifyResults *results;
} LGitCheckoutNotifyParams;

static int LGitCheckoutNotify(git_checkout_notify_t why,
							  const char *path,
							  const git_diff_file *baseline,
							  const git_diff_file *target,
							  const git_diff_file *workdir,
							  void *payload)
{
	LGitCheckoutNotifyResults *results = (LGitCheckoutNotifyResults*)payload;
	if (payload == NULL) {
		return GIT_EUSER;
	}
	switch (why) {
	case GIT_CHECKOUT_NOTIFY_CONFLICT:
		results->conflicts.insert(std::string(path));
		return 0;
	/* XXX: We may care about this one */
	/* case GIT_CHECKOUT_NOTIFY_DIRTY: // changed but no action */
	/* case: GIT_CHECKOUT_NOTIFY_NONE: // no action required */
	/* case: GIT_CHECKOUT_NOTIFY_UPDATED: // cleanly replaced */
	/* case: GIT_CHECKOUT_NOTIFY_UNTRACKED: */
	/* case: GIT_CHECKOUT_NOTIFY_IGNORED: */
	default:
		return 0;
	}
}

/* Allocate and set the callback */
SCCRTN LGitInitCheckoutNotifyCallbacks(LGitContext *ctx, HWND hwnd, git_checkout_options *co_opts)
{
	LGitCheckoutNotifyResults *results = new LGitCheckoutNotifyResults;
	if (results == NULL) {
		return SCC_E_UNKNOWNERROR;
	}
	/* XXX: Do we care about dirty? */
	co_opts->notify_flags = GIT_CHECKOUT_NOTIFY_CONFLICT;
	co_opts->notify_cb = LGitCheckoutNotify;
	co_opts->notify_payload = results;
	return SCC_OK;
}

/* could use rework if we change based on params to callback */
static LVCOLUMN path_column = {
	LVCF_TEXT | LVCF_WIDTH, 0, 125, "Path"
};

static void InitCheckoutNotifyView(HWND hwnd, LGitCheckoutNotifyParams* params)
{
	HWND lv = GetDlgItem(hwnd, IDC_CHECKOUT_NOTIFY_LIST);

	ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT
		| LVS_EX_HEADERDRAGDROP
		| LVS_EX_LABELTIP);
	ListView_SetUnicodeFormat(lv, TRUE);
	SendMessage(lv, WM_SETFONT, (WPARAM)params->ctx->listviewFont, TRUE);

	ListView_InsertColumn(lv, 0, &path_column);

	/* Initialize the system image list */
	HIMAGELIST sil = LGitGetSystemImageList();
	ListView_SetImageList(lv, sil, LVSIL_SMALL);
}

static void FillCheckoutNotifyView(HWND hwnd, LGitCheckoutNotifyParams* params)
{
	HWND lv = GetDlgItem(hwnd, IDC_CHECKOUT_NOTIFY_LIST);

	std::set<std::string> *conflicts = &params->results->conflicts;
	std::set<std::string>::iterator i;
	int index = 0;
	for (i = conflicts->begin(); i != conflicts->end(); i++) {
		SHFILEINFOW sfi;
		ZeroMemory(&sfi, sizeof(sfi));
		LVITEMW lvi;
		wchar_t buf[1024], absolute[1024];
		
		ZeroMemory(&lvi, sizeof(LVITEMW));
		lvi.mask = LVIF_TEXT | LVIF_IMAGE;
		LGitUtf8ToWide(i->c_str(), buf, 1024);
		/* need the absolute path for the icon unfortunately */
		wcslcpy(absolute, params->ctx->workdir_path_utf16, 1024);
		wcslcat(absolute, buf, 1024);
		LGitTranslateStringCharsW(absolute, L'/', L'\\');
		SHGetFileInfoW(absolute, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
		lvi.iImage = sfi.iIcon;
		lvi.pszText = buf;
		lvi.iItem = index++;
		lvi.iSubItem = 0;

		SendMessage(lv, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
		if (lvi.iItem == -1) {
			LGitLog(" ! ListView_InsertItem failed\n");
			continue;
		}
	}
	ListView_SetColumnWidth(lv, 0, LVSCW_AUTOSIZE_USEHEADER);
}

static BOOL CALLBACK CheckoutNotifyDialogProc(HWND hwnd,
											  unsigned int iMsg,
											  WPARAM wParam,
											  LPARAM lParam)
{
	LGitCheckoutNotifyParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitCheckoutNotifyParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitCheckoutNotifyView(hwnd, param);
		FillCheckoutNotifyView(hwnd, param);
		/* Finally beep like we're a system error */
		MessageBeep(MB_ICONERROR);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
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

/* Free and display any messages as needed */
SCCRTN LGitFinishCheckoutNotify(LGitContext *ctx, HWND hwnd, git_checkout_options *co_opts)
{
	if (co_opts->notify_payload == NULL) {
		return SCC_OK;;
	}
	LGitCheckoutNotifyResults *results = (LGitCheckoutNotifyResults*)co_opts->notify_payload;
	LGitCheckoutNotifyParams params;
	params.ctx = ctx;
	params.results = results;
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_CHECKOUT_NOTIFY),
		hwnd,
		CheckoutNotifyDialogProc,
		(LPARAM)&params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		return SCC_E_UNKNOWNERROR;
	default:
		break;
	}
	
	/* now free */
	delete results;
	co_opts->notify_payload = NULL;
	return SCC_OK;
}