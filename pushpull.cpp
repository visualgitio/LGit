/*
 * Exposes push and pull.
 */

#include "stdafx.h"
#include "LGit.h"

SCCRTN LGitPush(LGitContext *ctx, HWND hwnd, git_remote *remote, git_reference *ref)
{
	SCCRTN ret = SCC_OK;
	LGitLog("**LGitPush** Context=%p\n", ctx);
	if (remote == NULL) {
		LGitLog("!! Remote is null\n");
		return SCC_E_UNKNOWNERROR;
	}
	LGitLog("  remote %s\n", git_remote_name(remote));
	if (ref == NULL) {
		LGitLog("!! Ref is null\n");
		return SCC_E_UNKNOWNERROR;
	}
	const char *refspec = git_reference_symbolic_target(ref);
	if (refspec == NULL) {
		refspec = git_reference_name(ref);
	}
	LGitLog("     ref %s\n", refspec);
	const git_strarray refspecs = {
		(char**)&refspec,
		1
	};
	/* now push */
	git_push_options push_opts;
	git_push_options_init(&push_opts, GIT_PUSH_OPTIONS_VERSION);
	LGitInitRemoteCallbacks(ctx, hwnd, &push_opts.callbacks);
	LGitProgressInit(ctx, "Pushing to Remote", 0);
	LGitProgressStart(ctx, hwnd, TRUE);
	if (git_remote_push(remote, &refspecs, &push_opts) != 0) {
		LGitLibraryError(hwnd, "git_remote_push");
		ret = SCC_E_UNKNOWNERROR;
	}
	LGitProgressDeinit(ctx);
	if (push_opts.callbacks.payload != NULL) {
		free(push_opts.callbacks.payload);
	}
	return ret;
}

typedef struct _LGitPushDialogParams {
	LGitContext *ctx;
	/* outputs */
	git_remote *remote;
	char remote_name[128];
	git_reference *ref;
	char refspec_name[128];
} LGitPushDialogParams;

static void InitPushView(HWND hwnd, LGitPushDialogParams* params)
{
	const char *name;
	size_t i;
	/* First initialize remotes */
	HWND remote_box = GetDlgItem(hwnd, IDC_PUSH_REMOTE);
	if (remote_box == NULL) {
		LGitLog(" !! %x\n", GetLastError());
		return;
	}
	LGitLog(" ! Getting remotes for push (ctx %p)\n", params->ctx);
	git_strarray remotes;
	ZeroMemory(&remotes, sizeof(git_strarray));
	if (git_remote_list(&remotes, params->ctx->repo) != 0) {
		LGitLibraryError(hwnd, "git_remote_list");
		return;
	}
	LGitLog(" ! Got back %d remote(s)\n", remotes.count);
	for (i = 0; i < remotes.count; i++) {
		name = remotes.strings[i];
		LGitLog(" ! Adding remote %s\n", name);
		SendMessage(remote_box, CB_ADDSTRING, 0, (LPARAM)name);
	}
	git_strarray_dispose(&remotes);
	/* Then populate refspecs */
	HWND ref_box = GetDlgItem(hwnd, IDC_PUSH_REF);
	LGitLog(" ! Getting remotes for push (ctx %p)\n", params->ctx);
	git_strarray refs;
	ZeroMemory(&refs, sizeof(git_strarray));
	if (git_reference_list(&refs, params->ctx->repo) != 0) {
		LGitLibraryError(hwnd, "git_reference_list");
		return;
	}
	LGitLog(" ! Got back %d reference(s)\n", refs.count);
	for (i = 0; i < refs.count; i++) {
		name = refs.strings[i];
		LGitLog(" ! Adding reference %s\n", name);
		SendMessage(ref_box, CB_ADDSTRING, 0, (LPARAM)name);
	}
	git_strarray_dispose(&refs);
	/* select the first item */
	SendMessage(remote_box, CB_SETCURSEL, 0, 0);
	/* get the current branch, out of convenience */
	git_reference *head_ref;
	if (git_reference_lookup(&head_ref, params->ctx->repo, "HEAD") != 0) {
		LGitLog("!! Couldn't find HEAD\n");
		return;
	}
	name = git_reference_symbolic_target(head_ref);
	if (name == NULL) {
		name = git_reference_name(head_ref);
	}
	LGitLog(" ! Current refspec is %s\n", name);
	SetWindowText(ref_box, name);
	if (head_ref != NULL) {
		git_reference_free(head_ref);
	}
}

static BOOL ValidateAndSetParams(HWND hwnd, LGitPushDialogParams* params)
{
	GetDlgItemText(hwnd, IDC_PUSH_REMOTE, params->remote_name, 256);
	if (strlen(params->remote_name) == 0) {
		MessageBox(hwnd,
			"There was no remote given.",
			"Invalid Remote", MB_ICONERROR);
		return FALSE;
	}
	if (git_remote_lookup(&params->remote, params->ctx->repo, params->remote_name) != 0) {
		LGitLibraryError(hwnd, "git_remote_lookup");
		return FALSE;
	}
	/* try more elaborate lookups for references */
	GetDlgItemText(hwnd, IDC_PUSH_REF, params->refspec_name, 256);
	if (strlen(params->refspec_name) == 0) {
		MessageBox(hwnd,
			"There was no reference given.",
			"Invalid Reference", MB_ICONERROR);
		return FALSE;
	}
	if (git_reference_lookup(&params->ref, params->ctx->repo, params->refspec_name) != 0) {
		if (git_reference_dwim(&params->ref, params->ctx->repo, params->refspec_name) != 0) {
			LGitLibraryError(hwnd, "git_reference_dwim");
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL CALLBACK PushDialogProc(HWND hwnd,
									unsigned int iMsg,
									WPARAM wParam,
									LPARAM lParam)
{
	LGitPushDialogParams *param;
	/* TODO: We should try to derive a path from the URL until overriden */
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitPushDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitPushView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitPushDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			if (ValidateAndSetParams(hwnd, param)) {
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

SCCRTN LGitPushDialog(LGitContext *ctx, HWND hwnd)
{
	LGitLog("**LGitPushDialog** Context=%p\n", ctx);
	SCCRTN ret;
	LGitPushDialogParams params;
	ZeroMemory(&params, sizeof(LGitPushDialogParams));
	params.ctx = ctx;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_PUSH_UPSTAIRS),
		hwnd,
		PushDialogProc,
		(LPARAM)&params)) {
	case 0:
		LGitLog(" ! Uh-oh, dialog error\n");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	case 1:
		ret = SCC_I_OPERATIONCANCELED;
		goto fin;
	case 2:
		break;
	}
	ret = LGitPush(ctx, hwnd, params.remote, params.ref);
fin:
	if (params.ref != NULL) {
		git_reference_free(params.ref);
	}
	if (params.remote != NULL) {
		git_remote_free(params.remote);
	}
	return ret;
}