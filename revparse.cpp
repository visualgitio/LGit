/*
 * Generic dialogs for getting a revparse compatible string (or parsing it?)
 */

#include <stdafx.h>

typedef struct _LGitRevparseDialogParams {
	LGitContext *ctx;
	char spec[128];
	char title[128];
	git_object *obj;
	git_reference *ref;
} LGitRevparseDialogParams;

static void InitRevparseView(HWND hwnd, LGitRevparseDialogParams* params)
{
	if (strlen(params->title)) {
		SetWindowText(hwnd, params->title);
	};
	/* yeah, we should load it from the struct... */
	HWND ref_cb = GetDlgItem(hwnd, IDC_REVPARSE_SPEC);
	LGitPopulateReferenceComboBox(hwnd, ref_cb, params->ctx);
	SetWindowText(ref_cb, params->spec);
}

static BOOL SetRevparseParams(HWND hwnd, LGitRevparseDialogParams* params)
{
	GetDlgItemText(hwnd, IDC_REVPARSE_SPEC, params->spec, 128);
	int rc = git_revparse_ext(&params->obj, &params->ref, params->ctx->repo, params->spec);
	BOOL ret = FALSE;
	/* XXX: Should these be passed out? */;
	switch (rc) {
	case GIT_EAMBIGUOUS:
		MessageBox(hwnd,
			"The specification entered is ambiguous and needs to be more specific.",
			params->spec,
			MB_ICONERROR);
		break;
	case GIT_EINVALIDSPEC:
		MessageBox(hwnd,
			"The specification entered is invalid.",
			params->spec,
			MB_ICONERROR);
		break;
	case GIT_ENOTFOUND:
		MessageBox(hwnd,
			"The revision couldn't be found.",
			params->spec,
			MB_ICONERROR);
		break;
	case 0:
		ret = TRUE;
		break;
	default:
		LGitLibraryError(hwnd, "git_revparse_ext");
	}
	return ret;
}

static BOOL CALLBACK RevparseDialogProc(HWND hwnd,
										unsigned int iMsg,
										WPARAM wParam,
										LPARAM lParam)
{
	LGitRevparseDialogParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitRevparseDialogParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitRevparseView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitRevparseDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			if (SetRevparseParams(hwnd, param)) {
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

/**
 * A dialog that parses with revparse, but gives you the valid string instead.
 *
 * Caller provides the buffer, and can use it to suggest a name.
 */
SCCRTN LGitRevparseDialogString(LGitContext *ctx, HWND hwnd, const char *title, char *spec, size_t bufsz)
{
	LGitLog("**LGitRevparseDialogString** Context=%p\n", ctx);
	LGitLog("    spec %s\n", spec);
	LGitRevparseDialogParams rp_params;
	ZeroMemory(&rp_params, sizeof(LGitRevparseDialogParams));
	rp_params.ctx = ctx;
	strlcpy(rp_params.title, title, 128);
	strlcpy(rp_params.spec, spec, 128);
	SCCRTN ret = SCC_OK;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_REVPARSE),
		hwnd,
		RevparseDialogProc,
		(LPARAM)&rp_params)) {
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
fin:
	if (rp_params.obj != NULL) {
		git_object_free(rp_params.obj);
	}
	if (rp_params.ref != NULL) {
		git_reference_free(rp_params.ref);
	}
	return ret;
}

/**
 * A dialog that calls revparse for you. Caller frees libgit2 objects.
 *
 * XXX: Is above redundant?
 */
SCCRTN LGitRevparseDialog(LGitContext *ctx, HWND hwnd, const char *title, const char *suggested_spec, git_object **obj, git_reference **ref)
{
	LGitLog("**LGitRevparseDialog** Context=%p\n", ctx);
	LGitRevparseDialogParams rp_params;
	ZeroMemory(&rp_params, sizeof(LGitRevparseDialogParams));
	rp_params.ctx = ctx;
	strlcpy(rp_params.title, title, 128);
	strlcpy(rp_params.spec, suggested_spec, 128);
	SCCRTN ret = SCC_OK;
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_REVPARSE),
		hwnd,
		RevparseDialogProc,
		(LPARAM)&rp_params)) {
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
fin:
	if (obj != NULL) {
		*obj = rp_params.obj;
	} else {
		git_object_free(rp_params.obj);
	}
	if (ref != NULL) {
		*ref = rp_params.ref;
	} else {
		git_reference_free(rp_params.ref);
	}
	return ret;
}