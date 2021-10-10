/*
 * Dialog to create a commit. Used primarily from explorer UI, since the SCC
 * functions will create from the IDE.
 */

#include <stdafx.h>

#define COMMIT_DIALOG_MESSAGE_SIZE 0x1000

typedef struct _LGitCreateCommitDialogParams {
	LGitContext *ctx;
	/* XXX: Allow custom signatures */
	/* In system enc, because SCC. It should be conv in commit func */
	char message[COMMIT_DIALOG_MESSAGE_SIZE];
} LGitCreateCommitDialogParams;

static void InitCreateCommitDialog(HWND hwnd, LGitCreateCommitDialogParams *params)
{
	HWND edit = GetDlgItem(hwnd, IDC_COMMIT_CREATE_MESSAGE);
	LGitSetMonospaceFont(params->ctx, edit);

	/* For any provided messages (i.e. merge_msg) */
	SetWindowText(edit, params->message);
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

static void InitAmendParams(HWND hwnd, LGitCreateCommitDialogParams *params)
{
	git_object *obj = NULL;
	git_commit *commit = NULL;
	const char *old_msg;
	if (git_revparse_single(&obj, params->ctx->repo, "HEAD") != 0) {
		LGitLibraryError(hwnd, "git_revparse_single");
		goto fin;
	}
	if (git_object_peel((git_object**)&commit, obj, GIT_OBJECT_COMMIT) != 0) {
		LGitLibraryError(hwnd, "git_object_peel");
		goto fin;
	}
	/* Convert newlines; charset conversion is a concern, see commit.cpp. */
	old_msg = git_commit_message(commit);
	if (old_msg == NULL) {
		old_msg = "";
	}
	LGitLfToCrLf(params->message, old_msg, COMMIT_DIALOG_MESSAGE_SIZE);
fin:
	git_commit_free(commit);
	git_object_free(obj);
}

SCCRTN LGitCreateCommitDialog(LGitContext *ctx, HWND hwnd, BOOL amend_last, const char *proposed_message, git_index *proposed_index)
{
	LGitLog("**LGitCreateCommitDialog** Context=%p\n", ctx);
	LGitCreateCommitDialogParams cc_params;
	ZeroMemory(&cc_params, sizeof(LGitCreateCommitDialogParams));
	cc_params.ctx = ctx;
	git_buf merge_msg = {0, 0};
	int rc = git_repository_message(&merge_msg, ctx->repo);
	/* Temp buffer for any conversions */
	char temp_message[COMMIT_DIALOG_MESSAGE_SIZE];
	/* If we're amending, put as much of the previous commit we can in. */
	if (amend_last) {
		InitAmendParams(hwnd, &cc_params);
	}
	/* If we have a message (i.e. from revert), paste it in */
	if (proposed_message != NULL) {
		LGitLfToCrLf(temp_message, proposed_message, COMMIT_DIALOG_MESSAGE_SIZE);
		strlcat(cc_params.message, temp_message, COMMIT_DIALOG_MESSAGE_SIZE);
	}
	if (rc == 0) {
		if (strlen(cc_params.message)) {
			strlcat(cc_params.message, "\r\n", COMMIT_DIALOG_MESSAGE_SIZE);
		}
		/* XXX: Check if we haven't copied more than buf size */
		LGitLfToCrLf(temp_message, merge_msg.ptr, COMMIT_DIALOG_MESSAGE_SIZE);
		strlcat(cc_params.message, temp_message, COMMIT_DIALOG_MESSAGE_SIZE);
	} else if (rc != GIT_ENOTFOUND) {
		LGitLibraryError(hwnd, "git_repository_message");
	}
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
	/*
	 * If we have an index proposed to us (i.e. revert), use that; otherwise,
	 * use the stage since the user has been building changes there..
	 */
	git_index *index = NULL;
	git_signature *signature = NULL;
	if (proposed_index != NULL) {
		index = proposed_index;
	} else if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hwnd, "git_repository_error");
		ret = SCC_E_NONSPECIFICERROR;
		goto err;
	}
	if (!amend_last && LGitGetDefaultSignature(hwnd, ctx, &signature) != SCC_OK) {
		goto err;
	}
	if (amend_last) {
		ret = LGitCommitIndexAmendHead(hwnd, ctx, index, cc_params.message, NULL, NULL);
	} else {
		ret = LGitCommitIndex(hwnd, ctx, index, cc_params.message, signature, signature);
	}
err:
	git_buf_dispose(&merge_msg);
	if (proposed_index == NULL) {
		git_index_free(index);
	}
	git_signature_free(signature);
	return ret;
}