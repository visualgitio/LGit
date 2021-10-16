/*
 * Display versions and such.
 */

#include "stdafx.h"

#define LG_URL "https://visualgit.io"

#pragma comment(lib, "version.lib")

typedef struct _LGitAboutParams {
	LGitContext *ctx;
} LGitAboutParams;

static void InitAboutView(HWND hwnd, LGitAboutParams *params)
{
	int lg2_maj = 0, lg2_min = 0, lg2_rev = 0;
	DWORD rsrc_size = 0;
	HGLOBAL glbl = NULL;
	HRSRC rsrc = NULL;
	void *glbl_locked = NULL, *glbl_copy = NULL;
	VS_FIXEDFILEINFO *ver = NULL;
	UINT ver_len = sizeof(ver);
	/* Terrible: We must have a *writable* copy of the resource */
	rsrc = FindResource(params->ctx->dllInst,
		MAKEINTRESOURCE(VS_VERSION_INFO),
		MAKEINTRESOURCE(RT_VERSION));
	if (rsrc == NULL) {
		goto fin;
	}
	rsrc_size = SizeofResource(params->ctx->dllInst, rsrc);
	glbl = LoadResource(params->ctx->dllInst, rsrc);
	if (glbl == NULL) {
		goto fin;
	}
	glbl_locked = LockResource(glbl);
	if (glbl_locked == NULL) {
		goto fin;
	}
	glbl_copy = malloc(rsrc_size);
	if (glbl_copy == NULL) {
		goto fin;
	}
	memcpy(glbl_copy, glbl_locked, rsrc_size);
	VerQueryValue(glbl_copy, "\\", (void**)&ver, &ver_len);
	/* Now for libgit2 */
	if (git_libgit2_version(&lg2_maj, &lg2_min, &lg2_rev) != 0) {
		LGitLibraryError(hwnd, "git_libgit2_version");
	}
	char ver_text[256];
	_snprintf(ver_text, 256,
		"Visual Git %d.%d.%d.%d\r\nPowered by libgit2 %d.%d.%d",
		HIWORD(ver->dwProductVersionMS),
		LOWORD(ver->dwProductVersionMS),
		HIWORD(ver->dwProductVersionLS),
		LOWORD(ver->dwProductVersionLS),
		lg2_maj, lg2_min, lg2_rev);
	SetDlgItemText(hwnd, IDC_ABOUT_VERSION, ver_text);
fin:
	if (glbl_copy != NULL) {
		free(glbl_copy);
	}
	/* Unlock is unnecessary in Win32 */
	if (glbl != NULL) {
		FreeResource(glbl);
	}
}

static BOOL CALLBACK AboutDialogProc(HWND hwnd,
									 unsigned int iMsg,
									 WPARAM wParam,
									 LPARAM lParam)
{
	LGitAboutParams *param;
	param = (LGitAboutParams*)GetWindowLong(hwnd, GWL_USERDATA);
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitAboutParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitAboutView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ABOUT_WEB:
			ShellExecute(NULL, "open", LG_URL, NULL, NULL, SW_SHOWNORMAL);
			return TRUE;
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

void LGitAbout(HWND hwnd, LGitContext *ctx)
{
	LGitAboutParams params;
	params.ctx = ctx;
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_ABOUT),
		hwnd,
		AboutDialogProc,
		(LPARAM)&params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		break;
	default:
		break;
	}
}
