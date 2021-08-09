/*
 * Status of files, including property dialogs.
 */

#include "stdafx.h"

typedef struct _LGitPropsDialogParams {
	LGitContext *ctx;
	unsigned int flags;
	const char *path, *full_path;
} LGitPropsDialogParams;

static void FillProps(HWND hwnd, LGitPropsDialogParams *params)
{
	char title[256];
	_snprintf(title, 256, "Properties for %s", params->path);
	SetWindowText(hwnd, title);

	/* Metaprogramming! */
#define LGIT_CHECK_PROPS_FLAG(flag) if(params->flags & GIT_STATUS_##flag) \
	CheckDlgButton(hwnd, IDC_STATUS_##flag, BST_CHECKED);

	/* XXX: how many here are actually possible? */
	LGIT_CHECK_PROPS_FLAG(INDEX_NEW);
	LGIT_CHECK_PROPS_FLAG(INDEX_MODIFIED);
	LGIT_CHECK_PROPS_FLAG(INDEX_DELETED);
	LGIT_CHECK_PROPS_FLAG(INDEX_RENAMED);
	LGIT_CHECK_PROPS_FLAG(INDEX_TYPECHANGE);
	LGIT_CHECK_PROPS_FLAG(WT_NEW);
	LGIT_CHECK_PROPS_FLAG(WT_MODIFIED);
	LGIT_CHECK_PROPS_FLAG(WT_DELETED);
	LGIT_CHECK_PROPS_FLAG(WT_RENAMED);
	LGIT_CHECK_PROPS_FLAG(WT_TYPECHANGE);
	LGIT_CHECK_PROPS_FLAG(WT_UNREADABLE);
	LGIT_CHECK_PROPS_FLAG(IGNORED);
	LGIT_CHECK_PROPS_FLAG(CONFLICTED);
	
	/* XXX: Do we display properties for the last commit? History covers it? */
}

static void ShowSysFileProperties(const char *fullpath)
{
	SHELLEXECUTEINFO info;
	ZeroMemory(&info, sizeof(SHELLEXECUTEINFO));

	info.cbSize = sizeof info;
	info.lpFile = fullpath;
	info.nShow = SW_SHOW;
	info.fMask = SEE_MASK_INVOKEIDLIST;
	info.lpVerb = "properties";

	/* XXX: COM should be initialized, VS does it for us? */
	ShellExecuteEx(&info);
}

static BOOL CALLBACK PropsDialogProc(HWND hwnd,
									 unsigned int iMsg,
									 WPARAM wParam,
									 LPARAM lParam)
{
	LGitPropsDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitPropsDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		FillProps(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitPropsDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		case IDC_FILESYSPROPS:
			ShowSysFileProperties(param->full_path);
			return TRUE;
		}
		return FALSE;
	default:
		return FALSE;
	}
}

SCCRTN SccProperties (LPVOID context, 
					  HWND hWnd, 
					  LPCSTR lpFileName)
{
	LGitPropsDialogParams params;
	const char *raw_path;
	char path[1024];

	int rc;
	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccProperties** Context=%p\n", context);
	LGitLog("  %s\n", lpFileName);

	raw_path = LGitStripBasePath(ctx, lpFileName);
	if (raw_path == NULL) {
		LGitLog("     Couldn't get base path for %s\n", lpFileName);
		return SCC_E_NONSPECIFICERROR;
	}
	/* Translate because libgit2 operates with forward slashes */
	strlcpy(path, raw_path, 1024);
	LGitTranslateStringChars(path, '\\', '/');

	rc = git_status_file(&params.flags, ctx->repo, path);
	switch (rc) {
	case 0:
		break;
	case GIT_ENOTFOUND:
		LGitLog(" ! Not found from status\n");
		return SCC_E_FILENOTCONTROLLED;
	default:
		LGitLibraryError(NULL, "Query info");
		return SCC_E_NONSPECIFICERROR;
	}

	params.ctx = ctx;
	params.path = path;
	params.full_path = lpFileName;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_FILEPROPS),
		hWnd,
		PropsDialogProc,
		(LPARAM)&params)) {
	case 0:
		LGitLog(" ! Uh-oh, dialog error\n");
		return SCC_E_NONSPECIFICERROR;
	default:
		break;
	}

	return SCC_OK;
}