/*
 * Utility functions for GUI stuff.
 */

#include "stdafx.h"
#include "LGit.h"

void LGitSetWindowIcon(HWND hwnd, HINSTANCE inst, LPCSTR name)
{
	HICON bigIcon, smallIcon;
	bigIcon = (HICON)LoadImage(inst, name, IMAGE_ICON, 32, 32, LR_SHARED);
	smallIcon = (HICON)LoadImage(inst, name, IMAGE_ICON, 16, 16, LR_SHARED);
	if (bigIcon != NULL) {
		SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)bigIcon);
	}
	if (smallIcon != NULL) {
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)smallIcon);
	}
}

static int CALLBACK BrowseCallbackProc(HWND hwnd,
									   UINT uMsg,
									   LPARAM lParam,
									   LPARAM lpData)
{
	switch (uMsg) {
	case BFFM_INITIALIZED:
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
		break;
	}
	return 0;
}

BOOL LGitBrowseForFolder(HWND hwnd, const char *title, char *buf, size_t bufsz)
{
	char path[_MAX_PATH];
	strlcpy(path, buf, _MAX_PATH);
	BROWSEINFO bi;
	ZeroMemory(&bi, sizeof(BROWSEINFO));
	bi.lpszTitle = title;
	bi.ulFlags = BIF_RETURNONLYFSDIRS
		| BIF_RETURNFSANCESTORS
		| BIF_EDITBOX
		| BIF_NEWDIALOGSTYLE;
	/* callback to handle at least initializing the dialog */
    bi.lpfn = BrowseCallbackProc;
    bi.lParam = (LPARAM) path;
	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl == NULL) {
		return FALSE;
	}
	SHGetPathFromIDList(pidl, path);
	/* for the sake of updating */
	strlcpy(buf, path, bufsz);
	CoTaskMemFree(pidl);
	return TRUE;
}

void LGitPopulateRemoteComboBox(HWND parent, HWND cb, LGitContext *ctx)
{
	const char *name;
	size_t i;
	LGitLog(" ! Getting remotes for push (ctx %p)\n", ctx);
	git_strarray remotes;
	ZeroMemory(&remotes, sizeof(git_strarray));
	if (git_remote_list(&remotes, ctx->repo) != 0) {
		LGitLibraryError(parent, "git_remote_list");
		return;
	}
	LGitLog(" ! Got back %d remote(s)\n", remotes.count);
	for (i = 0; i < remotes.count; i++) {
		name = remotes.strings[i];
		LGitLog(" ! Adding remote %s\n", name);
		SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)name);
	}
	git_strarray_dispose(&remotes);
	/* select the first item */
	SendMessage(cb, CB_SETCURSEL, 0, 0);
}