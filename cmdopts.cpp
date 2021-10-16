/*
 * UI to control options for plugin commands and the plugin itself.
 */

#include "stdafx.h"

static SCCRTN LGitOptionsCaps(enum SCCCOMMAND cmd)
{
	switch (cmd)
	{
	/*
	 * We support commit (checkin, add, remove) and get for remote operations.
	 */
	case SCC_COMMAND_GET:
	case SCC_COMMAND_ADD:
	case SCC_COMMAND_REMOVE:
	case SCC_COMMAND_CHECKIN:
	case SCC_COMMAND_OPTIONS:
		return SCC_I_ADV_SUPPORT;
	default:
		return SCC_E_OPNOTSUPPORTED;
	}
}

static BOOL CALLBACK CommitOptsDialogProc(HWND hwnd,
										  unsigned int iMsg,
										  WPARAM wParam,
										  LPARAM lParam)
{
	LGitContext *param;
	/* TODO: We should try to derive a path from the URL until overriden */
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitContext*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		CheckDlgButton(hwnd, IDC_OPTIONS_COMMIT_PUSH, param->commitOpts.push);
		LGitLog(" ! Push is %d\n", param->commitOpts.push);
		return TRUE;
	case WM_COMMAND:
		param = (LGitContext*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			param->commitOpts.push = IsDlgButtonChecked(hwnd, IDC_OPTIONS_COMMIT_PUSH);
			LGitLog(" ! Push is now %d\n", param->commitOpts.push);
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

static SCCRTN SetCommitOptions(LGitContext *ctx, HWND hWnd, LPCMDOPTS *opts)
{
	if (opts != NULL && *opts == NULL) {
		*opts = &ctx->commitOpts;
		ZeroMemory(&ctx->commitOpts, sizeof(LGitCommitOpts));
	}
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_OPTIONS_COMMIT),
		hWnd,
		CommitOptsDialogProc,
		(LPARAM)ctx)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		break;
	case 1:
		return SCC_I_OPERATIONCANCELED;
	case 2:
		return SCC_OK;
	}
	return SCC_E_NONSPECIFICERROR;
}

static BOOL CALLBACK GetOptsDialogProc(HWND hwnd,
									   unsigned int iMsg,
									   WPARAM wParam,
									   LPARAM lParam)
{
	LGitContext *param;
	/* TODO: We should try to derive a path from the URL until overriden */
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitContext*)lParam;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		CheckDlgButton(hwnd, IDC_OPTIONS_COMMIT_PULL, param->getOpts.pull);
		LGitLog(" ! Pull is %d\n", param->getOpts.pull);
		return TRUE;
	case WM_COMMAND:
		param = (LGitContext*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDOK:
			param->getOpts.pull = IsDlgButtonChecked(hwnd, IDC_OPTIONS_COMMIT_PULL);
			LGitLog(" ! Pull is now %d\n", param->getOpts.pull);
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

static SCCRTN SetGetOptions(LGitContext *ctx, HWND hWnd, LPCMDOPTS *opts)
{
	if (opts != NULL && *opts == NULL) {
		*opts = &ctx->getOpts;
		ZeroMemory(&ctx->commitOpts, sizeof(LGitGetOpts));
	}
	switch (DialogBoxParamW(ctx->dllInst,
		MAKEINTRESOURCEW(IDD_OPTIONS_GET),
		hWnd,
		GetOptsDialogProc,
		(LPARAM)ctx)) {
	case 0:
	case -1:
		LGitLog(" ! Uh-oh, dialog error\n");
		break;
	case 1:
		return SCC_I_OPERATIONCANCELED;
	case 2:
		return SCC_OK;
	}
	return SCC_E_NONSPECIFICERROR;
}

static SCCRTN SetPluginOptions(LGitContext *ctx, HWND hWnd)
{
	/* The provided options aren't useful since this is our responsibility. */
	MessageBox(hWnd, "Not supported.", "Visual Git Options", MB_ICONERROR);
	return SCC_E_OPNOTSUPPORTED;
}

SCCRTN SccGetCommandOptions (LPVOID context, 
							 HWND hWnd, 
							 enum SCCCOMMAND nCommand,
							 LPCMDOPTS * ppvOptions)
{
	LGitLog("**SccGetCommandOptions** Context=%p\n", context);
	LGitLog("  command %s\n", LGitCommandName(nCommand));
	LGitLog("  options %p\n", ppvOptions);
	if (ppvOptions != NULL) {
		LGitLog("  *options %p\n", *ppvOptions);
	}
	LGitContext *ctx = (LGitContext*)context;

	/* IDE calls first with NULL to see if we support this option */
	if (ppvOptions == NULL) {
		return LGitOptionsCaps(nCommand);
	}
	/* Context may not be open. Don't do things needing repos from dialogs. */

	/* Dispatch to shared dialogs. */
	SCCRTN ret;
	switch (nCommand) {;
	case SCC_COMMAND_GET:
		ret = SetGetOptions(ctx, hWnd, ppvOptions);
		break;
	case SCC_COMMAND_ADD:
	case SCC_COMMAND_REMOVE:
	case SCC_COMMAND_CHECKIN:
		ret = SetCommitOptions(ctx, hWnd, ppvOptions);
		break;
	case SCC_COMMAND_OPTIONS:
		ret = SetPluginOptions(ctx, hWnd);
		break;
	default:
		ret = SCC_E_OPNOTSUPPORTED;
		break;
	}
	LGitLog(" ! New ptr %p rc %d\n", *ppvOptions, ret);
	return ret;
}