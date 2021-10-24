/*
 * Status of files, including property dialogs.
 */

#include "stdafx.h"

typedef struct _LGitPropsDialogParams {
	LGitContext *ctx;
	unsigned int flags;
	const char *path, *full_path;
	const wchar_t *path_utf16, *full_path_utf16;
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

static void ShowSysFileProperties(const wchar_t *fullpath)
{
	SHELLEXECUTEINFOW info;
	ZeroMemory(&info, sizeof(SHELLEXECUTEINFO));

	info.cbSize = sizeof info;
	info.lpFile = fullpath;
	info.nShow = SW_SHOW;
	info.fMask = SEE_MASK_INVOKEIDLIST;
	info.lpVerb = L"properties";

	/* XXX: COM should be initialized, VS does it for us? */
	ShellExecuteExW(&info);
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
			ShowSysFileProperties(param->full_path_utf16);
			return TRUE;
		}
		return FALSE;
	default:
		return FALSE;
	}
}

static SCCRTN ShowPropertiesDialog(LGitContext *ctx,
								   HWND hWnd,
								   const char *full_path,
								   const char *relative_path)
{
	LGitPropsDialogParams params;

	int rc = git_status_file(&params.flags, ctx->repo, relative_path);
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

	wchar_t relative_path_utf16[2048];
	LGitUtf8ToWide(relative_path, relative_path_utf16, 2048);
	wchar_t full_path_utf16[2048];
	LGitUtf8ToWide(full_path, full_path_utf16, 2048);

	params.ctx = ctx;
	params.path = relative_path;
	params.full_path = full_path;
	params.path_utf16 = relative_path_utf16;
	params.full_path_utf16 = full_path_utf16;
	params.entry = git_index_get_bypath(index, relative_path, 0);
	LGitLog(" ! Entry is %p\n", params.entry);

	PROPSHEETPAGEW psp[2];
	ZeroMemory(&psp[0], sizeof(PROPSHEETPAGEW));
	psp[0].dwSize = sizeof(PROPSHEETPAGEW);
	psp[0].hInstance = ctx->dllInst;
	psp[0].pszTemplate = MAKEINTRESOURCEW(IDD_FILEPROPS_FILE);
	psp[0].pfnDlgProc = GeneralDialogProc;
	psp[0].lParam = (LPARAM)&params;
	ZeroMemory(&psp[1], sizeof(PROPSHEETPAGEW));
	psp[1].dwSize = sizeof(PROPSHEETPAGEW);
	psp[1].hInstance = ctx->dllInst;
	psp[1].pszTemplate = MAKEINTRESOURCEW(IDD_FILEPROPS_STATUS);
	psp[1].pfnDlgProc = StatusDialogProc;
	psp[1].lParam = (LPARAM)&params;
	PROPSHEETHEADERW psh;
	ZeroMemory(&psh, sizeof(PROPSHEETHEADERW));
	psh.dwSize = sizeof(PROPSHEETHEADERW);
	psh.dwFlags =  PSH_PROPSHEETPAGE
		| PSH_NOAPPLYNOW
		| PSH_PROPTITLE
		| PSH_NOCONTEXTHELP
		| PSH_USECALLBACK;
	psh.pfnCallback = LGitImmutablePropSheetProc;
	psh.hwndParent = hWnd;
	psh.hInstance = ctx->dllInst;
	psh.pszCaption = relative_path_utf16;
	psh.nPages = 2;
	psh.ppsp = psp;

	PropertySheetW(&psh);

	if (index != NULL) {
		git_index_free(index);
	}
	return SCC_OK;
}

/**
 * Like SccProperties, but takes a relative libgit2 path.
 *
 * (It will construct a full path based on the workdir.
 */
SCCRTN LGitFileProperties(LGitContext *ctx, HWND hWnd, LPCSTR relative_path)
{
	char full_path[1024];
	strlcpy(full_path, ctx->workdir_path, 1024);
	strlcat(full_path, relative_path, 1024);
	LGitTranslateStringChars(full_path, '/', '\\');

	return ShowPropertiesDialog(ctx, hWnd, full_path, relative_path);
}

SCCRTN SccProperties (LPVOID context, 
					  HWND hWnd, 
					  LPCSTR lpFileName)
{
	const char *relative_path;
	char full_path[2048];

	LGitContext *ctx = (LGitContext*)context;

	LGitLog("**SccProperties** Context=%p\n", context);
	LGitLog("  %s\n", lpFileName);

	LGitAnsiToUtf8(lpFileName, full_path, 2048);
	if (full_path == NULL) {
		LGitLog("!! Couldn't alloc utf8 path str\n");
		return SCC_E_NONSPECIFICERROR;
	}

	relative_path = LGitStripBasePath(ctx, full_path);
	if (relative_path == NULL) {
		LGitLog("     Couldn't get base path for %s\n", lpFileName);
		return SCC_E_NONSPECIFICERROR;
	}
	/* Translate because libgit2 operates with forward slashes */
	LGitTranslateStringChars(full_path, '\\', '/');

	/* declaring here in case can use the wide conv from AnsiToUtf8Alloc */
	SCCRTN ret = ShowPropertiesDialog(ctx, hWnd, full_path, relative_path);
	return ret;
}