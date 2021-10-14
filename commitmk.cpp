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
	git_signature *author;
	git_signature *committer;
} LGitCreateCommitDialogParams;

static void SetSignatureWindowText(HWND hwnd, LGitCreateCommitDialogParams *params, UINT control, const git_signature *signature)
{
	wchar_t buf[512];
	LGitFormatSignatureWithTimeW(signature, buf, 512);
	SetDlgItemTextW(hwnd, control, buf);
}

static void InitCreateCommitDialog(HWND hwnd, LGitCreateCommitDialogParams *params)
{
	HWND edit = GetDlgItem(hwnd, IDC_COMMIT_CREATE_MESSAGE);
	LGitSetMonospaceFont(params->ctx, edit);

	/* For any provided messages (i.e. merge_msg) */
	SetWindowText(edit, params->message);
	SetSignatureWindowText(hwnd, params, IDC_COMMIT_CREATE_AUTHOR, params->author);
	SetSignatureWindowText(hwnd, params, IDC_COMMIT_CREATE_COMMITTER, params->committer);
}

static void ChangeSignature(HWND hwnd, LGitCreateCommitDialogParams *params, UINT control, git_signature **signature)
{
	char name[128], mail[128];
	if (*signature != NULL) {
		strlcpy(name, (*signature)->name, 128);
		strlcpy(mail, (*signature)->email, 128);
	} else {
		ZeroMemory(name, 128);
		ZeroMemory(mail, 128);
	}
	if (LGitSignatureDialog(params->ctx, hwnd, name, 128, mail, 128, FALSE) != SCC_OK) {
		return;
	}
	git_signature_free(*signature);
	*signature = NULL;
	if (git_signature_now(signature, name, mail) != 0) {
		LGitLibraryError(hwnd, "git_signature_now");
		return;
	}
	SetSignatureWindowText(hwnd, params, control, *signature);
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
		case IDC_COMMIT_CREATE_CHANGEAUTHOR:
			ChangeSignature(hwnd, param, IDC_COMMIT_CREATE_AUTHOR, &param->author);
			return TRUE;
		case IDC_COMMIT_CREATE_CHANGECOMMITTER:
			ChangeSignature(hwnd, param, IDC_COMMIT_CREATE_COMMITTER, &param->committer);
			return TRUE;
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

static BOOL InitFreshParams(HWND hwnd, LGitCreateCommitDialogParams *params)
{
	/* Since it can be called from partially initialized amend */
	git_signature_free(params->author);
	git_signature_free(params->committer);
	BOOL ret = FALSE;
	/* The commit message can be blank, but we do want the user's sig here */
	git_signature *signature = NULL;
	if (LGitGetDefaultSignature(hwnd, params->ctx, &signature) != SCC_OK) {
		goto fail;
	}
	if (git_signature_dup(&params->author, signature) != 0) {
		LGitLibraryError(hwnd, "Create Commit Dialog (Duplicating Author Signature)");
		goto fail;
	}
	if (git_signature_dup(&params->committer, signature) != 0) {
		LGitLibraryError(hwnd, "Create Commit Dialog (Duplicating Committer Signature)");
		goto fail;
	}
	ret = TRUE;
fail:
	git_signature_free(signature);
	return ret;
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
	/* Now copy the signatures */
	if (git_signature_dup(&params->author, git_commit_author(commit)) != 0) {
		LGitLibraryError(hwnd, "Create Commit Dialog (Duplicating Author Signature)");
		/* If this fails, just try using a default signature and punt */
		InitFreshParams(hwnd, params);
		goto fin;
	}
	if (git_signature_dup(&params->committer, git_commit_committer(commit)) != 0) {
		LGitLibraryError(hwnd, "Create Commit Dialog (Duplicating Committer Signature)");
		InitFreshParams(hwnd, params);
		goto fin;
	}
fin:
	git_commit_free(commit);
	git_object_free(obj);
}

static void InitRepoMessage(HWND hwnd, LGitCreateCommitDialogParams *params)
{
	git_buf merge_msg = {0, 0};
	/* Temp buffer for any conversions */
	char temp_message[COMMIT_DIALOG_MESSAGE_SIZE];
	ZeroMemory(temp_message, COMMIT_DIALOG_MESSAGE_SIZE);
	int rc = git_repository_message(&merge_msg, params->ctx->repo);
	if (rc == 0) {
		if (strlen(params->message)) {
			strlcat(params->message, "\r\n", COMMIT_DIALOG_MESSAGE_SIZE);
		}
		LGitLfToCrLf(temp_message, merge_msg.ptr, COMMIT_DIALOG_MESSAGE_SIZE);
		strlcat(params->message, temp_message, COMMIT_DIALOG_MESSAGE_SIZE);
	} else if (rc != GIT_ENOTFOUND) {
		LGitLibraryError(hwnd, "git_repository_message");
	}
	git_buf_dispose(&merge_msg);
}

SCCRTN LGitCreateCommitDialog(LGitContext *ctx, HWND hwnd, BOOL amend_last, const char *proposed_message, git_index *proposed_index)
{
	LGitLog("**LGitCreateCommitDialog** Context=%p\n", ctx);
	SCCRTN ret = SCC_OK;
	git_index *index = NULL;
	LGitCreateCommitDialogParams cc_params;
	ZeroMemory(&cc_params, sizeof(LGitCreateCommitDialogParams));
	cc_params.ctx = ctx;
	/* If we're amending, put as much of the previous commit we can in. */
	if (amend_last) {
		InitAmendParams(hwnd, &cc_params);
	} else if(!InitFreshParams(hwnd, &cc_params)) {
		goto err;
	}
	/* If we have a message (i.e. from revert), paste it in */
	InitRepoMessage(hwnd, &cc_params);
	if (proposed_message != NULL) {
		char temp_message[COMMIT_DIALOG_MESSAGE_SIZE];
		ZeroMemory(temp_message, COMMIT_DIALOG_MESSAGE_SIZE);
		LGitLfToCrLf(temp_message, proposed_message, COMMIT_DIALOG_MESSAGE_SIZE);
		strlcat(cc_params.message, temp_message, COMMIT_DIALOG_MESSAGE_SIZE);
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
	/*
	 * If we have an index proposed to us (i.e. revert), use that; otherwise,
	 * use the stage since the user has been building changes there..
	 */
	if (proposed_index != NULL) {
		index = proposed_index;
	} else if (git_repository_index(&index, ctx->repo) != 0) {
		LGitLibraryError(hwnd, "git_repository_error");
		ret = SCC_E_NONSPECIFICERROR;
		goto err;
	}
	if (amend_last) {
		ret = LGitCommitIndexAmendHead(hwnd, ctx, index, cc_params.message, NULL, NULL);
	} else {
		ret = LGitCommitIndex(hwnd, ctx, index, cc_params.message, cc_params.author, cc_params.committer);
	}
err:
	if (proposed_index == NULL) {
		git_index_free(index);
	}
	git_signature_free(cc_params.author);
	git_signature_free(cc_params.committer);
	return ret;
}