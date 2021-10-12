/*
 * Information about a commit.
 */

#include "stdafx.h"

typedef struct _LGitCommitInfoDialogParams {
	LGitContext *ctx;
	git_commit *commit;
	git_tag *tag;
} LGitCommitInfoDialogParams;

static void FillCommitView(HWND hwnd, LGitCommitInfoDialogParams *params)
{
	UINT codepage = LGitGitToWindowsCodepage(git_commit_message_encoding(params->commit));

	const git_oid *oid = git_commit_id(params->commit);
	char *oid_s = git_oid_tostr_s(oid);
	SetDlgItemText(hwnd, IDC_COMMITINFO_OID, oid_s);

	const git_signature *author, *committer;
	const char *message;
	author = git_commit_author(params->commit);
	committer = git_commit_committer(params->commit);
	message = git_commit_message(params->commit);

	wchar_t sig_msg[512];
	LGitFormatSignatureWithTimeW(author, sig_msg, 512);
	SetDlgItemTextW(hwnd, IDC_COMMITINFO_AUTHOR, sig_msg);
	LGitFormatSignatureWithTimeW(committer, sig_msg, 512);
	SetDlgItemTextW(hwnd, IDC_COMMITINFO_COMMITTER, sig_msg);

	/* set the font THEN prep the message */
	HWND message_box = GetDlgItem(hwnd, IDC_COMMITINFO_MESSAGE);
	LGitSetMonospaceFont(params->ctx, message_box);
	LGitSetWindowTextFromCommitMessage(message_box, codepage, message);
}

static void FillTagView(HWND hwnd, LGitCommitInfoDialogParams *params)
{
	UINT codepage = CP_UTF8; /* git tag has no encoding property */

	const git_oid *oid = git_tag_id(params->tag);
	char *oid_s = git_oid_tostr_s(oid);
	SetDlgItemText(hwnd, IDC_TAGINFO_OID, oid_s);

	const git_signature *author;
	const char *message;
	author = git_tag_tagger(params->tag);
	message = git_tag_message(params->tag);

	wchar_t sig_msg[512], sig_person[256];

	LGitTimeToStringW(&author->when, sig_msg, 512);
	wcslcat(sig_msg, L" ", 512);
	LGitFormatSignatureW(author, sig_person, 256);
	wcslcat(sig_msg, sig_person, 512);
	SetDlgItemTextW(hwnd, IDC_TAGINFO_AUTHOR, sig_msg);

	/* set the font THEN prep the message */
	HWND message_box = GetDlgItem(hwnd, IDC_TAGINFO_MESSAGE);
	LGitSetMonospaceFont(params->ctx, message_box);
	LGitSetWindowTextFromCommitMessage(message_box, CP_UTF8, message);
}

static BOOL CALLBACK CommitInfoDialogProc(HWND hwnd,
										  unsigned int iMsg,
										  WPARAM wParam,
										  LPARAM lParam)
{
	LGitCommitInfoDialogParams *param;
	PROPSHEETPAGE *psp;
	switch (iMsg) {
	case WM_INITDIALOG:
		psp = (PROPSHEETPAGE*)lParam;
		param = (LGitCommitInfoDialogParams*)psp->lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		FillCommitView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitCommitInfoDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
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

static BOOL CALLBACK TagInfoDialogProc(HWND hwnd,
									   unsigned int iMsg,
									   WPARAM wParam,
									   LPARAM lParam)
{
	LGitCommitInfoDialogParams *param;
	PROPSHEETPAGE *psp;
	switch (iMsg) {
	case WM_INITDIALOG:
		psp = (PROPSHEETPAGE*)lParam;
		param = (LGitCommitInfoDialogParams*)psp->lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		FillTagView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitCommitInfoDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
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

static void FillRefsView(HWND hwnd, LGitCommitInfoDialogParams *params)
{
	HWND lb = GetDlgItem(hwnd, IDC_COMMITINFO_REFERENCES);
	/* We need to get a list of refs, then see if the ref has the commit */
	git_strarray refs = {0,0};
	size_t i;
	const git_oid *this_oid = git_commit_id(params->commit);
	const git_oid *ref_oid = NULL;
	if (git_reference_list(&refs, params->ctx->repo) != 0) {
		LGitLibraryError(hwnd, "git_reference_iterator_new");
		return;
	}
	/* this can be slow; progress dialog and make quantifiable w/ list */
	LGitProgressInit(params->ctx, "Checking References for Commit", 0);
	/* XXX: make blocking since we can switch tabs between */
	LGitProgressStart(params->ctx, hwnd, TRUE);
	for (i = 0; i < refs.count; i++) {
		if (LGitProgressCancelled(params->ctx)) {
			/* We'll work with what we have. */
			break;
		}
		const char *name = refs.strings[i];
		git_reference *ref = NULL;
		if (git_reference_lookup(&ref, params->ctx->repo, name) != 0) {
			LGitLog("!! Failed to lookup %s\n", name);
			continue;
		}
		char msg[128];
		_snprintf(msg, 128, "Checking %s", name);
		LGitProgressText(params->ctx, msg, 1);
		LGitProgressSet(params->ctx, i, refs.count);
		/* If hard tag, get the commit first. It'll be null if not tag */
		ref_oid = git_reference_target_peel(ref);
		if (ref_oid == NULL) {
			ref_oid = git_reference_target(ref);
		}
		/* it's accepted if the ref_oid is NULL; i.e. origin/HEAD */
		if (ref_oid != NULL
			&& (git_oid_equal(ref_oid, this_oid) == 1
			|| git_graph_descendant_of(params->ctx->repo, ref_oid, this_oid) == 1)) {
			/* XXX: this could be a listview like refs dialog instead */
			SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)name);
		}
		if (ref != NULL) {
			git_reference_free(ref);
		}
	}
	LGitProgressDeinit(params->ctx);
	git_strarray_dispose(&refs);
}

static BOOL CALLBACK RefsDialogProc(HWND hwnd,
								    unsigned int iMsg,
								    WPARAM wParam,
								    LPARAM lParam)
{
	LGitCommitInfoDialogParams *param;
	PROPSHEETPAGE *psp;
	switch (iMsg) {
	case WM_INITDIALOG:
		psp = (PROPSHEETPAGE*)lParam;
		param = (LGitCommitInfoDialogParams*)psp->lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		FillRefsView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitCommitInfoDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
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

void LGitViewCommitInfo(LGitContext *ctx, HWND hWnd, git_commit *commit, git_tag *tag)
{
	LGitLog("**LGitViewCommitInfo** Context=%p\n", ctx);
	LGitCommitInfoDialogParams params;
	params.ctx = ctx;
	params.commit = commit;
	params.tag = tag;

	int page_count = 0;
	PROPSHEETPAGE psp[3];
	ZeroMemory(&psp[page_count], sizeof(PROPSHEETPAGE));
	psp[page_count].dwSize = sizeof(PROPSHEETPAGE);
	psp[page_count].hInstance = ctx->dllInst;
	psp[page_count].pszTemplate = MAKEINTRESOURCE(IDD_COMMITINFO);
	psp[page_count].pfnDlgProc = CommitInfoDialogProc;
	psp[page_count].lParam = (LPARAM)&params;
	page_count++;
	ZeroMemory(&psp[page_count], sizeof(PROPSHEETPAGE));
	psp[page_count].dwSize = sizeof(PROPSHEETPAGE);
	psp[page_count].hInstance = ctx->dllInst;
	psp[page_count].pszTemplate = MAKEINTRESOURCE(IDD_COMMITINFO_REFERENCES);
	psp[page_count].pfnDlgProc = RefsDialogProc;
	psp[page_count].lParam = (LPARAM)&params;
	if (tag != NULL) {
		page_count++;
		ZeroMemory(&psp[page_count], sizeof(PROPSHEETPAGE));
		psp[page_count].dwSize = sizeof(PROPSHEETPAGE);
		psp[page_count].hInstance = ctx->dllInst;
		psp[page_count].pszTemplate = MAKEINTRESOURCE(IDD_TAGINFO);
		psp[page_count].pfnDlgProc = TagInfoDialogProc;
		psp[page_count].lParam = (LPARAM)&params;
	}
	page_count++; /* for nPages */
	PROPSHEETHEADER psh;
	ZeroMemory(&psh, sizeof(PROPSHEETHEADER));
	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags =  PSH_PROPSHEETPAGE
		| PSH_NOAPPLYNOW
		| PSH_NOCONTEXTHELP
		| PSH_USECALLBACK;
	psh.pfnCallback = LGitImmutablePropSheetProc;
	psh.hwndParent = hWnd;
	psh.hInstance = ctx->dllInst;
	psh.pszCaption = "Commit Details";
	psh.nPages = page_count;
	psh.ppsp = psp;

	PropertySheet(&psh);
}