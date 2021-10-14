/*
 * Tags, but only adding. Managing is in tag.cpp...
 */

#include <stdafx.h>

typedef struct _LGitAddTagDialogParams {
	LGitContext *ctx;
	char new_name[128];
	char based_on[128];
	char message[0x1000]; /* should be cleaned up */
	BOOL force;
} LGitAddTagDialogParams;

static void InitTagAddView(HWND hwnd, LGitAddTagDialogParams* params)
{
	SetDlgItemText(hwnd, IDC_TAG_ADD_NAME, params->new_name);
	/* yeah, we should load it from the struct... */
	HWND ref_cb = GetDlgItem(hwnd, IDC_TAG_ADD_BASED_ON);
	LGitPopulateReferenceComboBox(hwnd, ref_cb, params->ctx);
	SetDlgItemText(hwnd, IDC_TAG_ADD_BASED_ON, "HEAD");
	SetDlgItemText(hwnd, IDC_TAG_ADD_MESSAGE, params->message);
	CheckDlgButton(hwnd, IDC_TAG_ADD_FORCE, params->force ? BST_CHECKED : BST_UNCHECKED);
}

static BOOL SetTagAddParams(HWND hwnd, LGitAddTagDialogParams* params)
{
	GetDlgItemText(hwnd, IDC_TAG_ADD_NAME, params->new_name, 128);
	GetDlgItemText(hwnd, IDC_TAG_ADD_BASED_ON, params->based_on, 128);
	GetDlgItemText(hwnd, IDC_TAG_ADD_MESSAGE, params->message, 0x1000);
	int valid = 0;
	/* if there's an error, then if it's valid */
	if (git_tag_name_is_valid(&valid, params->new_name) != 0) {
		LGitLibraryError(hwnd, "git_tag_name_is_valid");
		return FALSE;
	}
	if (!valid) {
		MessageBox(hwnd,
			"The tag name is invalid.",
			"Invalid Tag Name",
			MB_ICONWARNING);
		return FALSE;
	}
	/* check if the reference exists */
	git_object *obj = NULL;
	switch (git_revparse_single(&obj, params->ctx->repo, params->based_on)) {
	case GIT_ENOTFOUND:
		MessageBox(hwnd,
			"The reference name to base off of doesn't exist.",
			"Invalid Reference Name",
			MB_ICONWARNING);
		return FALSE;
	case GIT_EINVALIDSPEC:
		MessageBox(hwnd,
			"The reference name to base off of is invalid.",
			"Invalid Reference Name",
			MB_ICONWARNING);
		return FALSE;
	case GIT_EAMBIGUOUS:
		MessageBox(hwnd,
			"The revision given isn't specific enough.",
			"Invalid Revision",
			MB_ICONWARNING);
		return FALSE;
	case 0:
		git_object_free(obj);
		break;
	default:
		LGitLibraryError(hwnd, "git_revparse_single");
		return FALSE;
	}
	params->force = IsDlgButtonChecked(hwnd, IDC_TAG_ADD_FORCE) == BST_CHECKED;
	return TRUE;
}

static BOOL CALLBACK AddTagDialogProc(HWND hwnd,
										 unsigned int iMsg,
										 WPARAM wParam,
										 LPARAM lParam)
{
	LGitAddTagDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitAddTagDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitTagAddView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitAddTagDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			if (SetTagAddParams(hwnd, param)) {
				EndDialog(hwnd, 2);
			}
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

static SCCRTN CreateAnnotatedTag(LGitContext *ctx,
								 HWND hwnd,
								 const char *name,
								 git_commit *based_on,
								 const char *message,
								 BOOL force)
{
	SCCRTN ret = SCC_OK;
	git_oid tag_oid;
	git_signature *signature = NULL;
	git_buf prettified_message = {0, 0};
	const char *message_to_use = NULL;
	/* XXX: some of this logic is common to commit, share */
	if (git_message_prettify(&prettified_message, message, 1, '#') != 0) {
		LGitLog(" ! Failed to prettify commit, using original text\n");
		message_to_use = message;
	} else {
		message_to_use = prettified_message.ptr;
	}
	if (LGitGetDefaultSignature(hwnd, ctx, &signature) != SCC_OK) {
		goto fin;
	}
	switch (git_tag_create(&tag_oid, ctx->repo, name, (git_object*)based_on, signature, message_to_use, force)) {
	case 0:
		break;
	case GIT_EINVALIDSPEC:
		ret = SCC_E_NONSPECIFICERROR;
		MessageBox(hwnd,
			"The tag has an invalid name.",
			"Invalid Tag",
			MB_ICONERROR);
		goto fin;
	case GIT_EEXISTS:
		ret = SCC_E_NONSPECIFICERROR;
		MessageBox(hwnd,
			"The tag by that name already exists.",
			"Invalid Tag",
			MB_ICONERROR);
		goto fin;
	default:
		LGitLibraryError(hwnd, "git_tag_create");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	LGitLog(" ! Wrote tag %s as %s\n", name, git_oid_tostr_s(&tag_oid));
fin:
	git_buf_dispose(&prettified_message);
	if (signature != NULL) {
		git_signature_free(signature);
	}
	return ret;
}

static SCCRTN CreateLightweightTag(LGitContext *ctx,
								   HWND hwnd,
								   const char *name,
								   git_commit *based_on,
								   BOOL force)
{
	SCCRTN ret = SCC_OK;
	git_oid target_oid;
	switch (git_tag_create_lightweight(&target_oid, ctx->repo, name, (git_object*)based_on, force)) {
	case 0:
		break;
	case GIT_EINVALIDSPEC:
		ret = SCC_E_NONSPECIFICERROR;
		MessageBox(hwnd,
			"The tag has an invalid name.",
			"Invalid Tag",
			MB_ICONERROR);
		goto fin;
	case GIT_EEXISTS:
		ret = SCC_E_NONSPECIFICERROR;
		MessageBox(hwnd,
			"The tag by that name already exists.",
			"Invalid Tag",
			MB_ICONERROR);
		goto fin;
	default:
		LGitLibraryError(hwnd, "git_tag_create_lightweight");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	LGitLog(" ! Wrote tag %s to %s\n", name, git_oid_tostr_s(&target_oid));
fin:
	return ret;
}

SCCRTN LGitAddTagDialog(LGitContext *ctx, HWND hwnd)
{
	LGitLog("**LGitAddTagDialog** Context=%p\n", ctx);
	LGitAddTagDialogParams at_params;
	ZeroMemory(&at_params, sizeof(LGitAddTagDialogParams));
	at_params.ctx = ctx;
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_TAG_ADD),
		hwnd,
		AddTagDialogProc,
		(LPARAM)&at_params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		return SCC_E_NONSPECIFICERROR;
	case 1:
		return SCC_I_OPERATIONCANCELED;
	case 2:
		break;
	}
	/* Common stuff before we decide this is an annotated tag or not */
	SCCRTN ret = SCC_OK;
	git_object *object = NULL;
	git_commit *commit = NULL;
	/*
	 * XXX: Is it safe to make a branch without a commit (unborn); if so, just
	 * skip the init here and go to the branch create call.
	 */
	if (git_revparse_single(&object, ctx->repo, at_params.based_on) != 0) {
		LGitLibraryError(hwnd, "git_revparse_single");
		goto err;
	}
	if (git_object_peel((git_object**)&commit, object, GIT_OBJECT_COMMIT) != 0) {
		LGitLibraryError(hwnd, "git_object_peel");
		goto err;
	}
	/* Now decide. */
	if (strlen(at_params.message) == 0) {
		LGitLog(" ! Lightweight tag\n");
		ret = CreateLightweightTag(ctx, hwnd, at_params.new_name, commit, at_params.force);
	} else {
		LGitLog(" ! Annotated tag\n");
		ret = CreateAnnotatedTag(ctx, hwnd, at_params.new_name, commit, at_params.message, at_params.force);
	}
err:
	if (commit != NULL) {
		git_commit_free(commit);
	}
	if (object != NULL) {
		git_object_free(object);
	}
	return ret;
}