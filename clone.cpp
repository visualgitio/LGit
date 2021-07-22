/*
 * Git clone dialog.
 */

#include "stdafx.h"
#include "LGit.h"

typedef struct _LGitCloneDialogParams {
	LGitContext *ctx;
	/* Path is in/out, rest all out */
	char url[256];
	char path[_MAX_PATH];
	char branch[128];
	BOOL pathWritten;
} LGitCloneDialogParams;

static void InitCloneView(HWND hwnd, LGitCloneDialogParams* params)
{
	SetDlgItemText(hwnd, IDC_CLONE_PATH, params->path);
}

static int CALLBACK BrowseCallbackProc(HWND hwnd,
									   UINT uMsg,
									   LPARAM lParam,
									   LPARAM lpData)
{
	switch (uMsg) {
	case BFFM_INITIALIZED:
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
		break;
	}
	return 0;
}

static void BrowseForFolder(HWND hwnd, LGitCloneDialogParams* params)
{
	char path[_MAX_PATH];
	BROWSEINFO bi;
	ZeroMemory(&bi, sizeof(BROWSEINFO));
	bi.lpszTitle = "Browse for Repository Folder";
	bi.ulFlags = BIF_RETURNONLYFSDIRS
		| BIF_RETURNFSANCESTORS
		| BIF_EDITBOX
		| BIF_NEWDIALOGSTYLE;
	/* callback to handle at least initializing the dialog */
    bi.lpfn = BrowseCallbackProc;
	GetDlgItemText(hwnd, IDC_CLONE_PATH, params->path, MAX_PATH);
    bi.lParam = (LPARAM) path;
	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl == NULL) {
		return;
	}
	SHGetPathFromIDList(pidl, path);
	SetDlgItemText(hwnd, IDC_CLONE_PATH, path);
	/* for the sake of updating */
	strncpy(params->path, path, _MAX_PATH);
	CoTaskMemFree(pidl);
}

static BOOL ValidateAndSetParams(HWND hwnd, LGitCloneDialogParams* params)
{
	int rc, valid;
	GetDlgItemText(hwnd, IDC_CLONE_URL, params->url, 256);
	if (strlen(params->url) == 0) {
		MessageBox(hwnd,
			"There was no URL given to clone.",
			"Invalid URL", MB_ICONERROR);
		return FALSE;
	}
	GetDlgItemText(hwnd, IDC_CLONE_PATH, params->path, _MAX_PATH);
	if (strlen(params->path) == 0) {
		MessageBox(hwnd,
			"The path is empty.",
			"Invalid Path", MB_ICONERROR);
		return FALSE;
	}
	/*
	 * XXX: Validate that either everything leading up to this directory
	 * exists, or that the directory is empty.
	 */
	GetDlgItemText(hwnd, IDC_CLONE_BRANCH, params->branch, 128);
	/* Empty branch name -> default */
	if (strlen(params->branch) > 0) {
		rc = git_branch_name_is_valid(&valid, params->branch);
		if (rc != 0 || !valid) {
			MessageBox(hwnd,
				"The branch name is improperly formed.",
				"Invalid Branch", MB_ICONERROR);
			return FALSE;
		}
	}
	return TRUE;
}

#if 0
/**
 * Tries to append the most important component of the URL to the path.
 * Disabled because there's no way to call it from a dialog that I know of.
 */
static void BuildPath(HWND hwnd, LGitCloneDialogParams* params)
{
	HWND focused;
	if (params->pathWritten == TRUE) {
		return;
	}
	focused = GetFocus();
	if (GetDlgItem(hwnd, IDC_CLONE_PATH) == focused) {
		LGitLog(" ! Path written to custom, ignore it now\n");
		params->pathWritten = TRUE;
	} else if (GetDlgItem(hwnd, IDC_CLONE_URL) != focused) {
		return;
	}
	char new_path[_MAX_PATH], url[256], *url_begin, *suffix;
	GetDlgItemText(hwnd, IDC_CLONE_URL, url, 256);
	LGitLog("!! %s\n", url);
	url_begin = strrchr(url, '/');
	if (url_begin == NULL || strlen(url_begin) == 0) {
		return;
	}
	LGitLog("!! %s\n", url_begin);
	/* remove the .git often seen on SSH urls */
	suffix = strrchr(url_begin, '.');
	if (suffix != NULL) {
		suffix[0] = '\0';
	}
	LGitLog("!! %s\n", suffix);
	strncpy(new_path, params->path, _MAX_PATH);
	strcat(new_path, url_begin);
	SetDlgItemText(hwnd, IDC_CLONE_PATH, new_path);
}
#endif

static BOOL CALLBACK CloneDialogProc(HWND hwnd,
									 unsigned int iMsg,
									 WPARAM wParam,
									 LPARAM lParam)
{
	LGitCloneDialogParams *param;
	/* TODO: We should try to derive a path from the URL until overriden */
	switch (iMsg) {
	case WM_INITDIALOG:
		param = (LGitCloneDialogParams*)lParam;
		param->pathWritten = FALSE;
		SetWindowLong(hwnd, GWL_USERDATA, (long)param); /* XXX: 64-bit... */
		InitCloneView(hwnd, param);
		return TRUE;
	case WM_COMMAND:
		param = (LGitCloneDialogParams*)GetWindowLong(hwnd, GWL_USERDATA);
		switch (LOWORD(wParam)) {
		case IDC_CLONE_BROWSE:
			BrowseForFolder(hwnd, param);
			return TRUE;
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

SCCRTN LGitClone(LGitContext *ctx,
				 HWND hWnd,
				 LPSTR lpProjName, 
				 LPSTR lpLocalPath,
				 LPBOOL pbNew)
{
	SCCRTN ret = SCC_OK;
	/* The repository is created, but we'll re-open in SccOpenProject */
	git_repository *temp_repo;
	git_clone_options clone_opts;
	git_checkout_options co_opts;
	git_fetch_options fetch_opts;

	LGitCloneDialogParams params;

	git_checkout_options_init(&co_opts, GIT_CHECKOUT_OPTIONS_VERSION);
	co_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	git_fetch_options_init(&fetch_opts, GIT_FETCH_OPTIONS_VERSION);
	git_clone_options_init(&clone_opts, GIT_CLONE_OPTIONS_VERSION);
	clone_opts.checkout_opts = co_opts;
	LGitInitRemoteCallbacks(ctx, hWnd, &fetch_opts.callbacks);
	clone_opts.fetch_opts = fetch_opts;

	LGitLog(" ! Clone\n");

	ZeroMemory(&params, sizeof(LGitCloneDialogParams));
	params.ctx = ctx;
	strncpy(params.path, lpLocalPath, _MAX_PATH);
	switch (DialogBoxParam(ctx->dllInst,
		MAKEINTRESOURCE(IDD_CLONE),
		hWnd,
		CloneDialogProc,
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

	if (strlen(params.branch) > 0) {
		clone_opts.checkout_branch = params.branch;
	}

	/* Translate path for libgit2 */
	LGitTranslateStringChars(params.path, '\\', '/');
	if (git_clone(&temp_repo, params.url, params.path, &clone_opts) != 0) {
		LGitLibraryError(hWnd, "Repo Init");
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	git_repository_free(temp_repo);

	/* At least DevStudio wants backslashes */
	LGitTranslateStringChars(params.path, '/', '\\');
	char project[SCC_PRJPATH_SIZE];
	if (!LGitGetProjectNameFromPath(project, params.path, SCC_PRJPATH_SIZE)) {
		MessageBox(hWnd,
			"The project name couldn't be derived from the path.",
			"Error Cloning", MB_ICONERROR);
		ret = SCC_E_NONSPECIFICERROR;
		goto fin;
	}
	LGitLog(" ! Project name: %s\n", project);

	strncpy(lpProjName, project, SCC_PRJPATH_SIZE);
	strncpy(lpLocalPath, params.path, _MAX_PATH);
	/* XXX: Should we set pbNew? */

fin:
	if (fetch_opts.callbacks.payload != NULL) {
		free(fetch_opts.callbacks.payload);
	}
	return ret;
}