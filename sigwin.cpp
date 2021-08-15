/*
 * Dialog to get a signature. Used as a fallback for when libgit2 couldn't
 * find a signature, or perhaps for config editing later. Callers can use the
 * signature arguments for i.e. git_signature_now.
 */

#include "stdafx.h"

typedef struct _LGitSignatureParams {
	char name[128];
	char mail[128];
	BOOL useByDefault;
} LGitSignatureParams;

static BOOL CALLBACK SignatureDialogProc(HWND hwnd,
										 unsigned int iMsg,
										 WPARAM wParam,
										 LPARAM lParam)
{
	LGitSignatureParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitSignatureParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		return TRUE;
	case WM_COMMAND:
		param = (LGitSignatureParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			GetDlgItemText(hwnd, IDC_SIG_NAME, param->name, 128);
			GetDlgItemText(hwnd, IDC_SIG_MAIL, param->mail, 128);
			param->useByDefault = IsDlgButtonChecked(hwnd, IDC_SIGNATURE_MAKE_DEFAULT) == BST_CHECKED;
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

BOOL LGitSetSignature(LGitContext *ctx, HWND hwnd, const char *name, const char *mail)
{
	git_config *config = NULL;
	if (git_config_open_default(&config) != 0) {
		LGitLibraryError(hwnd, "git_config_default (set signature as default)");
		return FALSE;
	}
	int rc1, rc2;
	rc1 = git_config_set_string(config, "user.name", name);
	rc2 = git_config_set_string(config, "user.email", mail);
	if (rc1 != 0 || rc1 != 0) {
		LGitLibraryError(hwnd, "git_config_set_string (set signature as default)");
	}
	if (config != NULL) {
		git_config_free(config);
	}
	return rc1 == 0 && rc2 == 0;
}

BOOL LGitSignatureDialog(LGitContext *ctx,
						 HWND parent,
						 char *name,
						 size_t name_sz,
						 char *mail,
						 size_t mail_sz)
{
	LGitSignatureParams params;
	/*
	 * XXX: It might be a good idea to pre-initialize the dialog with some
	 * reasonable values filled in from i.e. current user and domain.
	 */
	if (name == NULL || name_sz < 1 || mail == NULL || mail_sz < 1) {
		return FALSE;
	}
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_NEW_SIGNATURE),
		parent,
		SignatureDialogProc,
		(LPARAM)&params)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		return FALSE;
	case 1:
		return FALSE;
	case 2:
		break;
	}
	LGitLog(" ! Signature name: %s\n", params.name);
	LGitLog(" ! Signature mail: %s\n", params.mail);
	strlcpy(name, params.name, name_sz);
	strlcpy(mail, params.mail, mail_sz);
	/*
	 * If the user checks this, then set it in the global (not repository
	 * level) config so they won't be asked again. Assumes this window is
	 * only invoked in contexts where that's not set.
	 */
	if (params.useByDefault) {
		LGitSetSignature(ctx, parent, params.name, params.mail);
	}
	return TRUE;
}