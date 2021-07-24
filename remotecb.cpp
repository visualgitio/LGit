/*
 * Callbacks used for git remote situations (auth, etc.), like in clones.
 */

#include "stdafx.h"
#include "LGit.h"

typedef struct _LGitRemoteParams {
	LGitContext *ctx;
	HWND parent;
	/* creds */
	git_credential **out;
	const char *url, *user_from_url;
	char username[128];
	char password[128];
} LGitRemoteParams;

static BOOL CALLBACK UserPassDialogProc(HWND hwnd,
										unsigned int iMsg,
										WPARAM wParam,
										LPARAM lParam)
{
	LGitRemoteParams *param;
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitRemoteParams*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		SetDlgItemText(hwnd, IDC_AUTH_USERPASS_DESC, param->url);
		if (param->user_from_url != NULL) {
			SetDlgItemText(hwnd, IDC_AUTH_USERNAME, param->user_from_url);
		} else {
			SetDlgItemText(hwnd, IDC_AUTH_USERNAME, param->ctx->username);
		}
		return TRUE;
	case WM_COMMAND:
		param = (LGitRemoteParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			GetDlgItemText(hwnd, IDC_AUTH_USERNAME, param->username, 128);
			GetDlgItemText(hwnd, IDC_AUTH_PASSWORD, param->password, 128);
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

static int UserPassDialog(LGitRemoteParams *params)
{
	switch (DialogBoxParam(params->ctx->dllInst,
		MAKEINTRESOURCE(IDD_AUTH_USERPASS),
		NULL,//params->parent,
		UserPassDialogProc,
		(LPARAM)params)) {
	case 0:
		LGitLog(" ! Uh-oh, dialog error\n");
		return GIT_EUSER;
	case 1:
		return 1;
	case 2:
		break;
	}
	LGitLog(" ! Username given: %s\n", params->username);
	LGitLog(" ! Password length: %d\n", strlen(params->password));
	int rc = git_credential_userpass_plaintext_new(params->out,
		params->username,
		params->password);
	LGitLog(" ! RC %x\n", rc);
	if (rc != 0) {
		LGitLibraryError(params->parent, "git_credential_userpass_plaintext_new");
	}
	return rc;
}

static int AcquireCredentials(git_credential **out,
							  const char *url,
							  const char *username_from_url,
							  unsigned int allowed_types,
							  void *payload)
{
	LGitLog("**AcquireCredentials**\n");
	LGitLog("      URL %s\n", url);
	LGitLog("  UserURL %s\n", username_from_url);
	LGitLog("    Types %x\n", allowed_types);
	LGitRemoteParams *params = (LGitRemoteParams*)payload;
	params->out = out;
	params->url = url;
	params->user_from_url;
	/* -1 error, 0 success, 1 cancel/fallthrough */
	int rc;
	/*
	 * If multiple types are supported, we should prompt to select them.
	 * However, most of these are mutually exclusive or unnecessary to
	 * implement here. Agent vs. specific path might be interesting, but both
	 * can come from GIT_CREDENTIAL_SSH_KEY. Plaintext and negotiate are both
	 * possible too, but cancelling will fall through to negotiate. (Reorder?)
	 */
	if (allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT) {
		LGitLog(" ! User/password\n");
		rc = UserPassDialog(params);
		if (rc == 0) {
			return rc;
		}
	}
	if (allowed_types & GIT_CREDENTIAL_SSH_KEY) {
		LGitLog(" ! SSH key\n");
		/* For now, just assume an agent. */
		rc = git_credential_ssh_key_from_agent(out, username_from_url);
		if (rc == 0) {
			return rc;
		}
	}
	if (allowed_types & GIT_CREDENTIAL_SSH_CUSTOM) {
		LGitLog(" ! SSH custom\n");
	}
	if (allowed_types & GIT_CREDENTIAL_DEFAULT) {
		LGitLog(" ! NTLM/Negotiate\n");
		rc = git_credential_default_new(out);
		if (rc == 0) {
			return rc;
		}
	}
	if (allowed_types & GIT_CREDENTIAL_SSH_INTERACTIVE) {
		LGitLog(" ! SSH interactive\n");
	}
	if (allowed_types & GIT_CREDENTIAL_USERNAME) {
		LGitLog(" ! SSH username only\n");
		rc = git_credential_username_new(out, params->ctx->username);
		if (rc == 0) {
			return rc;
		}
	}
	if (allowed_types & GIT_CREDENTIAL_SSH_MEMORY) {
		LGitLog(" ! SSH memory\n");
	}
	return 1;
}

static int LastChanceVerify(git_cert *cert,
							int valid,
							const char *host,
							void *payload)
{
	if (valid > 0) {
		/* why did it bother us then? */
		return 0;
	}
	LGitRemoteParams *params = (LGitRemoteParams*)payload;
	int ret = LGitCertificatePrompt(params->ctx, params->parent, cert, host);
	return ret == IDOK ? 0 : GIT_ECERTIFICATE;
}

void LGitInitRemoteCallbacks(LGitContext *ctx, HWND hWnd, git_remote_callbacks *cb)
{
	/* must be freed by caller */
	LGitRemoteParams *params = (LGitRemoteParams*)malloc(sizeof(LGitRemoteParams));
	params->parent = hWnd;
	params->ctx = ctx;

	git_remote_init_callbacks(cb, GIT_REMOTE_CALLBACKS_VERSION);
	cb->credentials = AcquireCredentials;
	cb->certificate_check = LastChanceVerify;
	/* XXX: Progress callbacks */
	cb->payload = params;
}