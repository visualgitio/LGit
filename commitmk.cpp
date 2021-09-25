/*
 * Dialog to create a commit. Used primarily from explorer UI, since the SCC
 * functions will create from the IDE.
 */

#include <stdafx.h>

typedef struct _LGitCreateCommitDialogParams {
	LGitContext *ctx;
	/* XXX: Allow custom signatures */
	/* In system enc, because SCC. It should be conv in commit func */
	char message[0x1000];
} LGitCreateCommitDialogParams;

static void InitCreateCommitDialog(HWND hwnd, LGitCreateCommitDialogParams *params)
{
	HWND edit = GetDlgItem(hwnd, IDC_COMMIT_CREATE_MESSAGE);
	LGitSetMonospaceFont(params->ctx, edit);
}

static BOOL CALLBACK CreateCommitDialogProc(HWND hwnd,
											unsigned int iMsg,
											WPARAM wParam,
											LPARAM lParam)
{
	LGitCreateCommitDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitCreateCommitDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		/*
		 * Perhaps we could show the changed files like git does, but it's
		 * likely the user invoked it from the Explorer window which shows
		 * the status of the stage.
		 */
		InitCreateCommitDialog(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitCreateCommitDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			GetDlgItemText(hwnd, IDC_COMMIT_CREATE_MESSAGE, param->message, 0x1000);
			EndDialog(hwnd, 2);
			return TRUE;
		case IDCANCEL:
			EndDialog(hwnd, 1);
			return TRUE;
		}
		return FALSE;
	default:
		return FALSE;
	}
}

SCCRTN LGitCreateCommitDialog(LGitContext *ctx, HWND hwnd)
{
	LGitLog("**LGitCreateCommitDialog** Context=%p\n", ctx);
	LGitCreateCommitDialogParams cc_params;
	ZeroMemory(&cc_params, sizeof(LGitCreateCommitDialogParams));
	cc_params.ctx = ctx;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_COMMIT_CREATE),
		hwnd,
		CreateCommitDialogProc,
		(LPARAM)&cc_params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		return SCC_E_NONSPECIFICERROR;
	case 1:
		return SCC_I_OPERATIONCANCELED;
	case 2:
		break;
	}
	SCCRTN ret = SCC_OK;
	git_index *index;
	if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hwnd, "git_repository_error");
		ret = SCC_E_NONSPECIFICERROR;
		goto err;
	}
	ret = LGitCommitIndex(hwnd, ctx, index, cc_params.message);;
err:
	return ret;
}