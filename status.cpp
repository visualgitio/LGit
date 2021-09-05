/*
 * Status of files, including property dialogs.
 */

#include "stdafx.h"

typedef struct _LGitPropsDialogParams {
	LGitContext *ctx;
	unsigned int flags;
	const char *path, *full_path;
	const git_index_entry *entry;
} LGitPropsDialogParams;

static void FillGeneral(HWND hwnd, LGitPropsDialogParams *params)
{
	if (params->entry == NULL) {
		return;
	}
}

static void FillStatus(HWND hwnd, LGitPropsDialogParams *params)
{
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

static BOOL CALLBACK StatusDialogProc(HWND hwnd,
									  unsigned int iMsg,
									  WPARAM wParam,
									  LPARAM lParam)
{
	LGitPropsDialogParams *param;
	PROPSHEETPAGE *psp;
	switch (iMsg) {
	case WM_INITDIALOG:
		psp = (PROPSHEETPAGE*)lParam;
		param = (LGitPropsDialogParams*)psp->lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		FillStatus(hwnd, param);
		return TRUE;
	default:
		return FALSE;
	}
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

static void UpdateFileInfo(HWND hwnd, LGitPropsDialogParams *params)
{
	const char *type = "Unknown";
	uint32_t mode = -1;
	if (params->entry == NULL) {
		type = "No stage entry";
		goto fin;
	}
	mode = params->entry->mode;
	LGitLog(" ! Mode is %x/%o", mode, mode);
	if ((mode & 0xE000) == 0xE000) {
		type = "Gitlink";
	} else if ((mode & 0xA000) == 0xA000) {
		type = "Symlink";
	} else if ((mode & 0x8000) == 0x8000) {
		type = (mode & 0111)
			? "Executable file"
			: "File";
	}
fin:
	SetDlgItemText(hwnd, IDC_FILEPROPS_STAGE_TYPE, type);
}

static BOOL CALLBACK GeneralDialogProc(HWND hwnd,
									   unsigned int iMsg,
									   WPARAM wParam,
									   LPARAM lParam)
{
	LGitPropsDialogParams *param;
	PROPSHEETPAGE *psp;
	switch (iMsg) {
	case WM_INITDIALOG:
		psp = (PROPSHEETPAGE*)lParam;
		param = (LGitPropsDialogParams*)psp->lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		UpdateFileInfo(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitPropsDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
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

	/* Get the index entry in case it turns out to be useful */
	git_index *index = NULL;
	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hWnd, "git_repository_index");
		return SCC_E_NONSPECIFICERROR;
	}

	params.ctx = ctx;
	params.path = path;
	params.full_path = lpFileName;
	params.entry = git_index_get_bypath(index, path, 0);
	LGitLog(" ! Param %p\n", &params);

	PROPSHEETPAGE psp[2];
	ZeroMemory(&psp[0], sizeof(PROPSHEETPAGE));
	psp[0].dwSize = sizeof(PROPSHEETPAGE);
	psp[0].hInstance = ctx->dllInst;
	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_FILEPROPS_FILE);
	psp[0].pfnDlgProc = GeneralDialogProc;
	psp[0].lParam = (LPARAM)&params;
	ZeroMemory(&psp[1], sizeof(PROPSHEETPAGE));
	psp[1].dwSize = sizeof(PROPSHEETPAGE);
	psp[1].hInstance = ctx->dllInst;
	psp[1].pszTemplate = MAKEINTRESOURCE(IDD_FILEPROPS_STATUS);
	psp[1].pfnDlgProc = StatusDialogProc;
	psp[1].lParam = (LPARAM)&params;
	PROPSHEETHEADER psh;
	ZeroMemory(&psh, sizeof(PROPSHEETHEADER));
	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags =  PSH_PROPSHEETPAGE
		| PSH_NOAPPLYNOW
		| PSH_PROPTITLE
		| PSH_NOCONTEXTHELP
		| PSH_USECALLBACK;
	psh.pfnCallback = LGitImmutablePropSheetProc;
	psh.hwndParent = hWnd;
	psh.hInstance = ctx->dllInst;
	psh.pszCaption = path;
	psh.nPages = 2;
	psh.ppsp = psp;

	PropertySheet(&psh);

	if (index != NULL) {
		git_index_free(index);
	}
	return SCC_OK;
}