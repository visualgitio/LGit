/*
 * Utility functions for GUI stuff.
 */

#include "stdafx.h"

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

/* primarily for status bars */
LONG LGitMeasureWidth(HWND measure_with, const char *text)
{
	/* make sure we're using the correct font when measuring */
	HDC dc = GetDC(measure_with);
	HFONT current_font = (HFONT)SendMessage(measure_with, WM_GETFONT, 0, 0);
	HGDIOBJ old_font;
	if (current_font != NULL) {
		old_font = SelectObject(dc, current_font);
	}
	if (dc == NULL) {
		return 0;
	}
	SIZE size = {0, 0};
	if (GetTextExtentPoint32(dc, text, strlen(text), &size) == 0) {
		return 0;
	}
	if (current_font != NULL) {
		/* restore */
		SelectObject(dc, old_font);
		/* XXX: Do we free current_font? */
	}
	ReleaseDC(measure_with, dc);
	return size.cx;
}

LONG LGitMeasureWidthW(HWND measure_with, const wchar_t *text)
{
	/* make sure we're using the correct font when measuring */
	HDC dc = GetDC(measure_with);
	HFONT current_font = (HFONT)SendMessage(measure_with, WM_GETFONT, 0, 0);
	HGDIOBJ old_font;
	if (current_font != NULL) {
		old_font = SelectObject(dc, current_font);
	}
	if (dc == NULL) {
		return 0;
	}
	SIZE size = {0, 0};
	if (GetTextExtentPoint32W(dc, text, wcslen(text), &size) == 0) {
		return 0;
	}
	if (current_font != NULL) {
		/* restore */
		SelectObject(dc, old_font);
		/* XXX: Do we free current_font? */
	}
	ReleaseDC(measure_with, dc);
	return size.cx;
}

void LGitUninitializeFonts(LGitContext *ctx)
{
	if (ctx->listviewFont != NULL) {
		DeleteObject((HGDIOBJ)ctx->listviewFont);
		ctx->listviewFont = NULL;
	}
	if (ctx->fixedFont != NULL) {
		DeleteObject((HGDIOBJ)ctx->fixedFont);
		ctx->fixedFont = NULL;
	}
}

void LGitInitializeFonts(LGitContext *ctx)
{
	LOGFONTW logfont;
	/* Clean */
	LGitUninitializeFonts(ctx);
	/*
	 * The reason behind this is because the font we may get in a ListView
	 * could be incapable of representing many Unicode characters.
	 */
	ZeroMemory(&logfont, sizeof(logfont));
	/* i'm guessing this is what's used for list view icons */
	SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(logfont), &logfont, 0);
	ctx->listviewFont = CreateFontIndirectW(&logfont);
	/* monospace */
	ZeroMemory(&logfont, sizeof(logfont));
	/* XXX: This isn't great on multimon */
	HDC dc = GetDC(NULL);
	logfont.lfHeight = -MulDiv(10, GetDeviceCaps(dc, LOGPIXELSY), 72);
	ReleaseDC(NULL, dc);
	logfont.lfCharSet = DEFAULT_CHARSET;
	logfont.lfPitchAndFamily = FIXED_PITCH;
	/* XXX: Make configurable. Could be called again if project has config */
	wcslcpy(logfont.lfFaceName, L"Courier New", 32);
	ctx->fixedFont = CreateFontIndirectW(&logfont);
	if (ctx->fixedFont == NULL) {
		ctx->fixedFont = (HFONT)GetStockObject(ANSI_FIXED_FONT);
	}
}

/* For compat. */
void LGitSetMonospaceFont(LGitContext *ctx, HWND ctrl)
{
	SendMessage(ctrl, WM_SETFONT, (WPARAM)ctx->fixedFont, TRUE);
}

void LGitControlFillsParentDialog(HWND hwnd, UINT dlg_item)
{
	RECT rect;
	HWND lv = GetDlgItem(hwnd, dlg_item);
	GetClientRect(hwnd, &rect);
	SetWindowPos(lv, NULL, 0, 0, rect.right, rect.bottom, 0);
}

void LGitControlFillsParentDialogCarveout(HWND hwnd, UINT dlg_item, RECT *bounds)
{
	RECT rect;
	HWND lv = GetDlgItem(hwnd, dlg_item);
	GetClientRect(hwnd, &rect);
	rect.left += bounds->left;
	rect.top += bounds->top;
	rect.right -= bounds->right;
	rect.bottom -= bounds->bottom;
	SetWindowPos(lv, NULL, 0, 0, rect.right, rect.bottom, 0);
}

BOOL LGitContextMenuFromSubmenu(HWND hwnd, HMENU menu, int position, int x, int y)
{
	/* are we in non-client area? */
	RECT clientArea;
	POINT pos = { x, y };
	GetClientRect(hwnd, &clientArea);
	ScreenToClient(hwnd, &pos);
	if (!PtInRect(&clientArea, pos)) {
		return FALSE;
	}
	/* this should be the "commit" menu */
	HMENU commitMenu = GetSubMenu(menu, position);
	TrackPopupMenu(commitMenu, TPM_LEFTALIGN, x, y, 0, hwnd, NULL);
	/* do we need to free GetSubMenu items? */
	return TRUE;
}

/**
 * Used to hide the OK button and mark the cancel button as close. Used for
 * property sheets representing immutable data, where having two buttons is
 * just confusing.
 */
BOOL CALLBACK LGitImmutablePropSheetProc(HWND hwnd,
										 unsigned int iMsg,
										 LPARAM lParam)
{
	if (iMsg == PSCB_INITIALIZED) {
		ShowWindow(GetDlgItem(hwnd, IDOK), SW_HIDE);
		SetWindowText(GetDlgItem(hwnd, IDCANCEL), "Close");
		return TRUE;
	}
	return FALSE;
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

BOOL LGitBrowseForFolder(HWND hwnd, const wchar_t *title, wchar_t *buf, size_t bufsz)
{
	wchar_t path[_MAX_PATH];
	wcslcpy(path, buf, _MAX_PATH);
	BROWSEINFOW bi;
	ZeroMemory(&bi, sizeof(BROWSEINFO));
	bi.lpszTitle = title;
	bi.ulFlags = BIF_RETURNONLYFSDIRS
		| BIF_RETURNFSANCESTORS
		| BIF_EDITBOX
		| BIF_NEWDIALOGSTYLE;
	/* callback to handle at least initializing the dialog */
    bi.lpfn = BrowseCallbackProc;
    bi.lParam = (LPARAM) path;
	LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
	if (pidl == NULL) {
		return FALSE;
	}
	SHGetPathFromIDListW(pidl, path);
	/* for the sake of updating */
	wcslcpy(buf, path, bufsz);
	CoTaskMemFree(pidl);
	return TRUE;
}

void LGitPopulateRemoteComboBox(HWND parent, HWND cb, LGitContext *ctx)
{
	size_t i;
	LGitLog(" ! Getting remotes for push (ctx %p)\n", ctx);
	git_strarray remotes;
	ZeroMemory(&remotes, sizeof(git_strarray));
	if (git_remote_list(&remotes, ctx->repo) != 0) {
		LGitLibraryError(parent, "git_remote_list");
		return;
	}
	LGitLog(" ! Got back %d remote(s)\n", remotes.count);
	/* clean out in case of stale entries */
	SendMessage(cb, CB_RESETCONTENT, 0, 0);
	for (i = 0; i < remotes.count; i++) {
		wchar_t name[1024];
		LGitUtf8ToWide(remotes.strings[i], name, 1024);
		LGitLog(" ! Adding remote %S\n", name);
		SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)name);
	}
	git_strarray_dispose(&remotes);
	/* select the first item */
	SendMessage(cb, CB_SETCURSEL, 0, 0);
}

void LGitPopulateReferenceComboBox(HWND parent, HWND cb, LGitContext *ctx)
{
	LGitLog(" ! Getting references (ctx %p)\n", ctx);
	git_reference_iterator *iter = NULL;
	git_reference *ref = NULL;
	if (git_reference_iterator_new(&iter, ctx->repo) != 0) {
		LGitLibraryError(parent, "Creating Reference Iterator");
		return;
	}
	/* clean out in case of stale entries */
	SendMessage(cb, CB_RESETCONTENT, 0, 0);
	/* add HEAD */
	int icon, index, rc;
	index = SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)"HEAD");
	icon = 4; /* checked out */
	SendMessage(cb, CB_SETITEMDATA, index, icon);
	while ((rc = git_reference_next(&ref, iter)) == 0) {
		wchar_t name[1024];
		LGitUtf8ToWide(git_reference_name(ref), name, 1024);
		LGitLog(" ! Adding ref %S\n", name);
		index = SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)name);
		icon = LGitGetIconForRef(ctx, ref);
		SendMessage(cb, CB_SETITEMDATA, index, icon);
	}
	if (rc != GIT_ITEROVER) {
		LGitLibraryError(parent, "git_reference_next");
		return;
	}
	git_reference_iterator_free(iter);
	/* Unlike remotes, don't necessarily select first, could use HEAD */
}

/* not strictly necessary to cache it seems. problem is LV dtor destroys it */
static HIMAGELIST sil;
static BOOL sil_init = FALSE;

HIMAGELIST LGitGetSystemImageList()
{
	if (sil_init) {
		return sil;
	}
	SHFILEINFOW sfi;
	wchar_t windir[MAX_PATH];
	ZeroMemory(&sfi, sizeof(sfi));
	/* this is so we get a dir that we know exists */
	GetWindowsDirectoryW(windir, MAX_PATH);
	/* does the path matter? */
	sil = (HIMAGELIST)SHGetFileInfoW(
		windir,
		0,
		&sfi,
		sizeof(SHFILEINFOW),
		SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	sil_init = TRUE;
	return sil;
}

/* culled from http://www.catch22.net/tuts/win32/drive-list-control */

#ifndef ODS_NOFOCUSRECT
#define ODS_NOFOCUSRECT 0x200
#endif

static int GetLineHeight(HWND hwndCombo, HFONT hFont, int nBitmapHeight)
{
	TEXTMETRICW     tm;
	HANDLE          hOldFont = NULL;
	HDC             hdc;

	int nTextHeight;
	int nLineHeight;

	hdc = GetDC(hwndCombo);

	hOldFont = SelectObject(hdc, hFont);
	GetTextMetricsW(hdc, &tm);
	SelectObject(hdc, hOldFont);
	ReleaseDC(hwndCombo, hdc);

	nTextHeight = tm.tmHeight + tm.tmInternalLeading;

	nLineHeight = max(nBitmapHeight, nTextHeight);

	return nLineHeight;
}

BOOL LGitMeasureIconComboBoxItem(HWND hwnd, UINT uCtrlId, MEASUREITEMSTRUCT *mis)
{
	/* this function is called BEFORE InitDialog sets the param */
    HFONT font = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
	/* therefore, we must hope this hasn't changed, bc can't get it */
	int bitmapHeight = GetSystemMetrics(SM_CXSMICON);
    mis->itemHeight = GetLineHeight(hwnd, font, bitmapHeight);
    return TRUE;
}

BOOL LGitDrawIconComboBox(LGitContext *ctx, HIMAGELIST il, HWND hwnd, UINT uCtrlId, DRAWITEMSTRUCT *dis)
{
    HWND hwndCombo = GetDlgItem(hwnd, uCtrlId);
    wchar_t szText[MAX_PATH+1];
    int idx;
	/* XXX: If we can make sure Measure can use ctx, we can cache it there */
	int bitmapYOffset = (dis->rcItem.bottom - dis->rcItem.top - (ctx->bitmapHeight-1)) / 2;

    switch(dis->itemAction)
    {
    case ODA_FOCUS:
        if (!(dis->itemState & ODS_NOFOCUSRECT))
            DrawFocusRect(dis->hDC, &dis->rcItem);
        break;

    case ODA_SELECT:
    case ODA_DRAWENTIRE:

		ZeroMemory(szText, 8);
        SendMessageW(hwndCombo, CB_GETLBTEXT, dis->itemID, (LONG)szText);

        if (dis->itemState & ODS_SELECTED)
        {
            SetTextColor(dis->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetBkColor(dis->hDC, GetSysColor(COLOR_HIGHLIGHT));
        }
        else
        {
            SetTextColor(dis->hDC, GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(dis->hDC, GetSysColor(COLOR_WINDOW));
        }

        /* the user data is set with the right index */
        idx = dis->itemData;

		/* icon size + padding */
		int displacement = ctx->bitmapHeight + 4;
        ExtTextOutW(dis->hDC, dis->rcItem.left + displacement, dis->rcItem.top+1, 
                   ETO_OPAQUE, &dis->rcItem, szText, wcslen(szText), 0);

        if (dis->itemState & ODS_FOCUS) 
        {
            if(!(dis->itemState & ODS_NOFOCUSRECT))
                DrawFocusRect(dis->hDC, &dis->rcItem);
        }

        ImageList_Draw(il, idx, dis->hDC, 
            dis->rcItem.left + 2, 
            dis->rcItem.top + bitmapYOffset, 
            ILD_TRANSPARENT);

        break;
    }

    return TRUE;
}

int LGitGetIconForRef(LGitContext *ctx, git_reference *ref)
{
	int icon;
	if (git_branch_is_head(ref)) {
		/* XXX: git_branch_is_checked_out? */
		icon = 4;
	} else if (git_reference_is_remote(ref)) {
		icon = 1;
	} else if (git_reference_is_branch(ref)) {
		icon = 0;
	} else if (git_reference_is_tag(ref)) {
		/* peeled target unavail from iter, reopen to see if annotated */
		git_object *ptr = NULL;
		/* XXX: is this expensive? */
		const git_oid *target = git_reference_target(ref);
		if (git_object_lookup(&ptr, ctx->repo, target, GIT_OBJECT_TAG) != 0) {
			icon = 3; /* annotated */
		} else {
			icon = 2; /* lightweight */
		}
		git_object_free(ptr);
	} else if (git_reference_is_note(ref)) {
		icon = 5;
	} else {
		icon = 6;
	}
	return icon;
}