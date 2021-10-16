/*
 * Exposes push and pull.
 */

#include "stdafx.h"

static int FetchHeadForeach(const char *ref,
							const char *url,
							const git_oid *oid,
							unsigned int is_merge,
							void *payload)
{
	LGitLog(" ! FetchHeadForeach ref %s url %s merge %d\n", ref, url, is_merge);
	if (is_merge != 0) {
		git_oid *this_oid = (git_oid*)payload;
		memcpy(this_oid, oid, sizeof(git_oid));
	}
	return 0;
}

static SCCRTN LGitPullMerge(LGitContext *ctx, HWND hwnd)
{
	SCCRTN ret = SCC_OK;
	git_oid oid;
	git_annotated_commit *ann_remote_head = NULL;
	int rc;
	LGitLog("**LGitPullMerge** Context=%p\n", ctx);

	/* XXX: Can this return multiple? */
	rc = git_repository_fetchhead_foreach(ctx->repo, FetchHeadForeach, &oid);
	switch (rc) {
	case GIT_ENOTFOUND:
		/* nothing to do */
		goto fin;
	case 0:
		break;
	default:
		LGitLibraryError(hwnd, "git_repository_fetchhead_foreach");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	/* XXX: git_annotated_commit_from_fetchhead? */
	if (git_annotated_commit_lookup(&ann_remote_head, ctx->repo, &oid) != 0) {
		LGitLibraryError(hwnd, "git_annotated_commit_lookup");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	ret = LGitMerge(ctx, hwnd, ann_remote_head);
fin:
	if (ann_remote_head != NULL) {
		git_annotated_commit_free(ann_remote_head);
	}
	return ret;
}

/**
 * Fetches, then optionally merges the current branch's tracking branch..
 */
SCCRTN LGitPull(LGitContext *ctx, HWND hwnd, git_remote *remote, LGitPullStrategy strategy)
{
	SCCRTN ret = SCC_OK;
	LGitLog("**LGitPull** Context=%p\n", ctx);
	if (remote == NULL) {
		LGitLog("!! Remote is null\n");
		return SCC_E_UNKNOWNERROR;
	}
	LGitLog("  remote %s\n", git_remote_name(remote));
	LGitLog("  strategy %d\n", strategy);

	git_fetch_options fetch_opts;
	git_fetch_options_init(&fetch_opts, GIT_FETCH_OPTIONS_VERSION);
	LGitInitRemoteCallbacks(ctx, hwnd, &fetch_opts.callbacks);
	LGitProgressInit(ctx, "Fetching from Remote", 0);
	LGitProgressStart(ctx, hwnd, TRUE);
	/*
	 * XXX: this will pull everything from the remote without a refspec. this
	 * might be slow for big repositories with lots of references.
	 */
	int rc = git_remote_fetch(remote, NULL, &fetch_opts, NULL);
	LGitProgressDeinit(ctx);
	if (rc != 0) {
		LGitLibraryError(hwnd, "git_remote_pull");
		ret = SCC_E_UNKNOWNERROR;
		goto fin;
	}
	LGitProgressDeinit(ctx);
	if (fetch_opts.callbacks.payload != NULL) {
		free(fetch_opts.callbacks.payload);
	}
	if (strategy == LGPS_FETCH) {
		goto fin;
	}
	ret = LGitPullMerge(ctx, hwnd);
fin:
	return ret;
}

typedef struct _LGitPullDialogParams {
	LGitContext *ctx;
	/* outputs */
	git_remote *remote;
	char remote_name[128];
	BOOL merge;
} LGitPullDialogParams;

static void InitPullView(HWND hwnd, LGitPullDialogParams* params)
{
	/* First initialize remotes */
	HWND remote_box = GetDlgItem(hwnd, IDC_PULL_REMOTE);
	if (remote_box == NULL) {
		LGitLog(" !! %x\n", GetLastError());
		return;
	}
	LGitPopulateRemoteComboBox(hwnd, remote_box, params->ctx);
}

static BOOL LGitPullDialogValidateAndSetParams(HWND hwnd, LGitPullDialogParams* params)
{
	wchar_t buf[128];
	GetDlgItemTextW(hwnd, IDC_PULL_REMOTE, buf, 128);
	LGitWideToUtf8(buf, params->remote_name, 128);
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
	params->merge = IsDlgButtonChecked(hwnd, IDC_PULL_FETCH) == BST_CHECKED;
	return TRUE;
}

static BOOL CALLBACK PullDialogProc(HWND hwnd,
									unsigned int iMsg,
									WPARAM wParam,
									LPARAM lParam)
{
	LGitPullDialogParams *param;
	/* TODO: We should try to derive a path from the URL until overriden */
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitPullDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		CheckDlgButton(hwnd, IDC_PULL_FETCH, BST_CHECKED);
		InitPullView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitPullDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDC_PULL_MANAGE_REMOTES:
			LGitShowRemoteManager(param->ctx, hwnd);
			InitPullView(hwnd, param);
			return TRUE;
		case IDOK:
			if (LGitPullDialogValidateAndSetParams(hwnd, param)) {
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

SCCRTN LGitPullDialog(LGitContext *ctx, HWND hwnd)
{
	LGitLog("**LGitPullDialog** Context=%p\n", ctx);
	SCCRTN ret;
	LGitPullDialogParams params;
	ZeroMemory(&params, sizeof(LGitPullDialogParams));
	params.ctx = ctx;
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_PULL),
		hwnd,
		PullDialogProc,
		(LPARAM)&params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	case 1:
		ret = SCC_I_OPERATIONCANCELED;
		goto fin;
	case 2:
		break;
	}
	ret = LGitPull(ctx, hwnd, params.remote, params.merge ? LGPS_MERGE_TO_HEAD : LGPS_FETCH);
fin:
	if (params.remote != NULL) {
		git_remote_free(params.remote);
	}
	return ret;
}

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

static void InitPushView(HWND hwnd, LGitPushDialogParams* params, BOOL do_refspecs)
{
	const char *name;
	wchar_t name_utf16[128];
	/* First initialize remotes */
	HWND remote_box = GetDlgItem(hwnd, IDC_PUSH_REMOTE);
	if (remote_box == NULL) {
		LGitLog(" !! %x\n", GetLastError());
		return;
	}
	LGitPopulateRemoteComboBox(hwnd, remote_box, params->ctx);
	if (!do_refspecs) {
		return;
	}
	/* Then populate refspecs */
	HWND ref_box = GetDlgItem(hwnd, IDC_PUSH_REF);
	LGitPopulateReferenceComboBox(hwnd, ref_box, params->ctx);
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
	LGitUtf8ToWide(name, name_utf16, 128);
	SetWindowTextW(ref_box, name_utf16);
	if (head_ref != NULL) {
		git_reference_free(head_ref);
	}
}

static BOOL LGitPushDialogValidateAndSetParams(HWND hwnd, LGitPushDialogParams* params)
{
	wchar_t buf[128];
	GetDlgItemTextW(hwnd, IDC_PUSH_REMOTE, buf, 128);
	LGitWideToUtf8(buf, params->remote_name, 128);
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
	GetDlgItemTextW(hwnd, IDC_PUSH_REF, buf, 128);
	LGitWideToUtf8(buf, params->refspec_name, 128);
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
		InitPushView(hwnd, param, TRUE);
		return TRUE;
	case WM_COMMAND:
		param = (LGitPushDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDC_PUSH_MANAGE_REMOTES:
			LGitShowRemoteManager(param->ctx, hwnd);
			InitPushView(hwnd, param, FALSE);
			return TRUE;
		case IDOK:
			if (LGitPushDialogValidateAndSetParams(hwnd, param)) {
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
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_PUSH_UPSTAIRS),
		hwnd,
		PushDialogProc,
		(LPARAM)&params)) {
	case 0:
	case -1:
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