/*
 * Information about a commit.
 */

#include "stdafx.h"

typedef struct _LGitCommitInfoDialogParams {
	LGitContext *ctx;
	git_commit *commit;
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

	wchar_t sig_msg[512], sig_person[256];
	wchar_t *new_msg_conv = NULL;

	LGitTimeToStringW(&author->when, sig_msg, 512);
	wcslcat(sig_msg, L" ", 512);
	LGitFormatSignatureW(author, sig_person, 256);
	wcslcat(sig_msg, sig_person, 512);
	SetDlgItemTextW(hwnd, IDC_COMMITINFO_AUTHOR, sig_msg);

	LGitTimeToStringW(&committer->when, sig_msg, 512);
	wcslcat(sig_msg, L" ", 512);
	LGitFormatSignatureW(committer, sig_person, 256);
	wcslcat(sig_msg, sig_person, 512);
	SetDlgItemTextW(hwnd, IDC_COMMITINFO_COMMITTER, sig_msg);

	/* set the font THEN prep the message */
	HFONT font = (HFONT)GetStockObject(ANSI_FIXED_FONT);
	HWND message_box = GetDlgItem(hwnd, IDC_COMMITINFO_MESSAGE);
	SendMessage(message_box, WM_SETFONT, (WPARAM)font, TRUE);

	/* we need to convert newlines */
	size_t len = strlen(message);
	char *message_converted = (char*)calloc(len, 2);
	for (size_t i = 0, j = 0; i < len; i++) {
		/* XXX: could be faster? */
		if (message[i] == '\n') {
			message_converted[j] = '\r';
			message_converted[j + 1] = '\n';
			j += 2;
		} else {
			message_converted[j++] = message[i];
		}
	}
	/* then convert to UCS-2 */
	int new_len = MultiByteToWideChar(codepage, 0, message_converted, -1, NULL, 0);
	if (new_len > 0) {
		/* enjoy your mojibake */
		SetWindowText(message_box, message_converted);
		goto not_unicode;
	}
	new_msg_conv = (wchar_t*)calloc(new_len + 1, sizeof(wchar_t));
	if (new_msg_conv == NULL) {
		SetWindowText(message_box, message_converted);
		goto not_unicode;
	}
	MultiByteToWideChar(codepage, 0, message_converted, -1, new_msg_conv, new_len + 1);
	SetWindowTextW(message_box, new_msg_conv);
	free(new_msg_conv);
not_unicode:
	free(message_converted);
}

static BOOL CALLBACK CommitInfoDialogProc(HWND hwnd,
										  unsigned int iMsg,
										  WPARAM wParam,
										  LPARAM lParam)
{
	LGitCommitInfoDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitCommitInfoDialogParams*)lParam;
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

void LGitViewCommitInfo(LGitContext *ctx, HWND hWnd, git_commit *commit)
{
	LGitLog("**LGitViewCommitInfo** Context=%p\n", ctx);
	LGitCommitInfoDialogParams params;
	params.ctx = ctx;
	params.commit = commit;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_COMMITINFO),
		hWnd,
		CommitInfoDialogProc,
		(LPARAM)&params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		break;
	default:
		break;
	}
}